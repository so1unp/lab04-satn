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

#include "estacion.h"
#include "header/defaultconfig.h"
#include <communication.h>

#define QUEUE_PERMISSIONS 0666

/* --- Variables Globales --- */
station my_station;

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
    
    while (a_station->fuel > 0) {
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

/**
 * @brief Rutina principal del hilo de la estación. Controla el consumo y notifica la baja al servidor.
 * @param arg Puntero genérico que se castea a la estructura de la estación objetivo.
 */
void* run_station(void* arg) {  
    station* a_station = (station*) arg;
    
    printf("[Estación] Hilo corriendo en posición (%d, %d). Iniciando consumo...\n", 
           a_station->pos_x, a_station->pos_y);
    
    consume_fuel(a_station);
    
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
    
    // Bloquea el proceso principal hasta que la estación termine sus operaciones
    pthread_join(thread_station, NULL);

    printf("\nFinalizando proceso.\n");
    return 0;
}