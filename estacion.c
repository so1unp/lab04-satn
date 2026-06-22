/**
 * @file estacion.c
 * @brief Proceso que representa y gestiona el ciclo de vida de una estación espacial.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <errno.h>

// --- Cabecera para semáforos POSIX ---
#include <semaphore.h>

#include "estacion.h"
#include "header/defaultconfig.h"
#include <communication.h>

#define QUEUE_PERMISSIONS 0666

/* --- Variables Globales --- */
station my_station;

// Variables globales para el manejo de interacciones con las naves
mqd_t cola_intercambio_estacion = -1;
char name_queue_estacion[60];
sem_t sem_hangar; // Semáforo local que garantiza un límite estricto de máximo 3 naves concurrentes

/* --- Definición de Funciones --- */

/**
 * @brief Inicializa los valores por defecto de la estructura de la estación.
 * @param a_station Puntero a la estructura de la estación a inicializar.
 * @param pos_x Coordenada inicial en el eje X asignada por el servidor.
 * @param pos_y Coordenada inicial en el eje Y asignada por el servidor.
 */
void inicializar_estacion(station *a_station, int pos_x, int pos_y) {
    if (a_station == NULL) return;
    
    a_station->pos_x = pos_x;
    a_station->pos_y = pos_y;
    a_station->fuel = 100; 
    a_station->fuelWarningSent = false;
    a_station->isDestroyed = false;
    
    // Limpieza de PIDs en el hangar
    for(int i = 0; i < MAX_HANGAR_CAPACITY; i++) {
        a_station->shipPids[i] = 0;
    }
}

/**
 * @brief Abre la cola de mensajes del servidor de manera No Bloqueante y envía una alerta de estado.
 * @param a_station Puntero a la estación que origina la alerta.
 */
void send_message_servidor(station *a_station) {
    if (a_station == NULL) {
        printf("Error: El puntero a_station es NULL\n");
        return;
    }
    
    mqd_t id_queue_mss;

    // Abre la cola de mensajes del servidor en modo solo escritura y No Bloqueante
    id_queue_mss = mq_open(SERVER_STATION_WARNING_COMMUNICATION_QUEUE_PATH, O_WRONLY | O_NONBLOCK); 
    if (id_queue_mss == (mqd_t)-1) {
        perror("Error al abrir la cola de mensajes del servidor");
        return;
    }

    // Preparación del paquete de datos
    msg_communication_station_warning msg;
    msg.stationPid = getpid();
    msg.pos_x_station = a_station->pos_x;
    msg.pos_y_station = a_station->pos_y;
    
    // === CORRECCIÓN: Ajuste al nombre real de la variable de combustible en la advertencia ===
    msg.fuel_station = a_station->fuel;

    printf("\n[Enviando al Servidor] PID: %d | Pos: (%d, %d) | Fuel: %d%%\n", 
           msg.stationPid, msg.pos_x_station, msg.pos_y_station, msg.fuel_station);

    // Envío del mensaje casteado a const char* bajo el estándar POSIX
    if (mq_send(id_queue_mss, (const char *)&msg, sizeof(msg_communication_station_warning), 0) == -1) {
        if (errno == EAGAIN) {
            printf("Error: La cola de mensajes del servidor está llena. No se pudo enviar.\n");
        } else {
            perror("Error al enviar el mensaje mediante mq_send");
        }
        mq_close(id_queue_mss);
        return;
    }

    printf("¡Mensaje enviado con éxito!\n");
    mq_close(id_queue_mss);   
}

/**
 * @brief Simula la tasa de consumo de combustible restando unidades de forma periódica.
 * @param a_station Puntero a la estación que consumirá combustible.
 */
void consume_fuel(station *a_station) {
    if (a_station == NULL) {
        perror("The station is null");
        exit(-1);
    }
    
    while (a_station->fuel > 0 && !a_station->isDestroyed) {
        sleep(2); // Demora el consumo simulando el tiempo transcurrido (2 segundos)
        
        a_station->fuel -= 10; 
        
        // Control de subdesbordamiento de combustible
        if (a_station->fuel < 0) {
            a_station->fuel = 0;
        }

        printf("[Estación] Combustible restante: %d%%\n", a_station->fuel);
        fflush(stdout);
    }
}

// --- Hilo de atención e intercambios con las naves ---
/**
 * @brief Atiende las solicitudes directas de las naves en el hangar de la estación.
 * Procesa recursos entregados por la nave para recargar la estación y reabastece a la nave.
 */
void* run_exchange_handler(void* arg) {
    station* est = (station*) arg;
    
    // Generamos un nombre único para la cola de la estación basado en sus coordenadas
    snprintf(name_queue_estacion, sizeof(name_queue_estacion), "/mq_estacion_%d_%d", est->pos_x, est->pos_y);
    
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 5;
    
    // === CORRECCIÓN: Mapear al tipo de struct correcto (msg_intercambio_solicitud) ===
    attr.mq_msgsize = sizeof(msg_intercambio_solicitud); 
    attr.mq_curmsgs = 0;
    
    // Forzamos unlink por seguridad antes de abrir de cero
    mq_unlink(name_queue_estacion);
    cola_intercambio_estacion = mq_open(name_queue_estacion, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS, &attr);
    if (cola_intercambio_estacion == (mqd_t)-1) {
        perror("[Estación] Error crítico al crear la cola de atención directa de naves");
        exit(1);
    }

    // Inicializamos el semáforo contador de control de hangar local
    sem_init(&sem_hangar, 0, MAX_HANGAR_CAPACITY);
    printf("[Estación] Canal de intercambio abierto: %s. Hangar disponible (Max 3 naves).\n", name_queue_estacion);

    // === CORRECCIÓN: Cambiar tipo no declarado por msg_intercambio_solicitud ===
    msg_intercambio_solicitud solicitud;
    
    while (!est->isDestroyed) {
        // Escucha llamadas bloqueantes de cualquier nave que pida un trueque
        if (mq_receive(cola_intercambio_estacion, (char*)&solicitud, sizeof(solicitud), NULL) == -1) {
            if (errno == EINTR) continue; // Manejo de interrupciones del sistema
            perror("[Estación] Error leyendo mensaje de la nave");
            break;
        }

        printf("[Estación] Petición de intercambio recibida del PID: %d\n", solicitud.shipPid);

        // 1. Añadimos el combustible recuperado por los minerales que trae la nave
        // === CORRECCIÓN: Adaptación de campos basados en lo enviado por la nave (recursosEntregados) ===
        int recursos_totales = solicitud.recursosEntregados;
        
        est->fuel += (recursos_totales * 2); // Factor de conversión: 2% de fuel por unidad
        if (est->fuel > 100) est->fuel = 100;
        
        printf("[Estación] Materiales procesados. Combustible actual de estación recuperado al: %d%%\n", est->fuel);

        // 2. Respondemos de forma privada abriendo el buzón específico de la nave
        char name_queue_nave[60];
        snprintf(name_queue_nave, sizeof(name_queue_nave), "%s%d", SHIP_BASE_PATH, solicitud.shipPid);
        
        mqd_t cola_privada_nave = mq_open(name_queue_nave, O_WRONLY);
        if (cola_privada_nave != (mqd_t)-1) {
            
            // === CORRECCIÓN: Responder usando el tipo estructurado msg_intercambio_respuesta esperado por la nave ===
            msg_intercambio_respuesta respuesta;
            respuesta.intercambioExitoso = true;
            respuesta.nuevoCombustibleNave = 100; // Recarga completa
            respuesta.nuevoOxigenoNave = 100;      // Recarga completa

            if (mq_send(cola_privada_nave, (const char*)&respuesta, sizeof(respuesta), 0) == -1) {
                perror("[Estación] Error al enviar respuesta de confirmación a la nave");
            } else {
                printf("[Estación] Transacción completada con éxito. Nave PID %d reabastecida al 100%%.\n", solicitud.shipPid);
            }
            mq_close(cola_privada_nave);
        } else {
            perror("[Estación] No se pudo abrir la cola privada de la nave para responder");
        }

        // 3. Liberamos el slot en el semáforo local. La nave se marcha del hangar.
        sem_post(&sem_hangar);
    }

    return NULL;
}

/**
 * @brief Rutina principal del hilo de la estación. Controla el consumo y notifica la baja al servidor.
 * @param arg Puntero genérico que se castea a la estructura de la estación objetivo.
 */
void* run_station(void* arg) {  
    station* a_station = (station*) arg;
    
    printf("[Estación] Hilo corriendo en posición (%d, %d). Iniciando consumo...\n", 
           a_station->pos_x, a_station->pos_y);
    
    consume_fuel(a_station);
    
    a_station->isDestroyed = true;
    printf("[Estación] ¡Alerta! Se agotó el combustible. Cerrando estación.\n");
    
    // Notifica el cierre por falta de insumos al servidor principal
    send_message_servidor(a_station);
    
    return NULL;  
}

/**
 * @brief Punto de entrada principal para el proceso de lanzamiento de la estación.
 */
int main(int argc, char *argv[]) {
    // Control de parámetros de entrada mínimos requeridos
    if (argc < CANT_ARGUMENTS) {   
        perror("uso: estacion <pos_x> <pos_y> \n");
        exit(1); 
    }

    int pos_x = atoi(argv[1]);
    int pos_y = atoi(argv[2]);
    
    if (pos_x < 0 || pos_y < 0) {
        printf("Error: Las posiciones no pueden ser negativas.\n");
        return 1;
    }
    
    printf("Started station\n");
    inicializar_estacion(&my_station, pos_x, pos_y);
    
    // Lanzamiento y gestión del hilo operativo de la estación
    pthread_t thread_station;
    if (pthread_create(&thread_station, NULL, run_station, &my_station) != 0) {
        perror("Error creating thread station");
        exit(1);
    }
    
    // Lanzamiento del hilo de intercambio con naves
    pthread_t thread_exchange;
    if (pthread_create(&thread_exchange, NULL, run_exchange_handler, &my_station) != 0) {
        perror("Error al crear el hilo despachador de intercambios");
        exit(1);
    }
    
    // Bloquea el proceso principal hasta que la estación termine sus operaciones por falta de fuel
    pthread_join(thread_station, NULL);

    // Limpieza final de recursos IPC
    pthread_cancel(thread_exchange); 
    pthread_join(thread_exchange, NULL);
    
    if (cola_intercambio_estacion != (mqd_t)-1) {
        mq_close(cola_intercambio_estacion);
        mq_unlink(name_queue_estacion);
    }
    sem_destroy(&sem_hangar);

    printf("\nFinalizando proceso de la Estación de manera limpia.\n");
    return 0;
}