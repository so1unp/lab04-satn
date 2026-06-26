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

/* --- Hilos de la Estacion --- */
pthread_t thread_station;

/* --- Colas de comunicacion --- */
mqd_t cola_inicializacion = -1;
mqd_t cola_aviso_poco_combustible = -1;


/* --- Definición de Funciones --- */

/**
 * @brief Inicializa las colas de mensajes.
 */
void conectar_ipc() {

    // Abre la cola de mensajes del servidor
    cola_aviso_poco_combustible = mq_open(SERVER_STATION_WARNING_COMMUNICATION_QUEUE_PATH, O_WRONLY | O_NONBLOCK); 
    if (cola_aviso_poco_combustible == (mqd_t)-1) {
        perror("Error al abrir la cola de mensajes del servidor");
        exit(1);
    }
    
    cola_inicializacion = mq_open(SERVER_CLIENT_INITIALIZATION_QUEUE_PATH, O_WRONLY);
    if (cola_inicializacion == (mqd_t)-1) {
        perror("Error abriendo cola de inicializacion");
        exit(1);
    }

}


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
void send_warning_message_servidor(station *a_station) {
    if (a_station == NULL) {
        printf("Error: El puntero a_station es NULL\n");
        return;
    }

    // Preparación del paquete de datos
    msg_communication_station_warning msg;
    msg.station_pid = getpid();
    msg.pos_x_station = a_station->pos_x;
    msg.pos_y_station = a_station->pos_y;
    msg.fuel_left = a_station->fuel;

    printf("\n[Enviando al Servidor] PID: %d | Pos: (%d, %d) | Fuel: %d%%\n", msg.station_pid, msg.pos_x_station, msg.pos_y_station, msg.fuel_left);

    // Envío del mensaje casteado a const char* bajo el estándar POSIX
    if (mq_send(cola_aviso_poco_combustible, (const char *)&msg, sizeof(msg_communication_station_warning), 0) == -1) {
        if (errno == EAGAIN) {
            printf("Error: La cola de mensajes del servidor está llena. No se pudo enviar.\n");
        } else {
            perror("Error al enviar el mensaje mediante mq_send");
        }
        mq_close(cola_aviso_poco_combustible);
        return;
    }

    printf("¡Mensaje enviado con éxito!\n");
    mq_close(cola_aviso_poco_combustible);   
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
        int tiempo_espera = TIME_OF_CONSUME_FUEL + (rand() % 3);
        sleep(tiempo_espera); // En vez de usar un tiempo fijo de consumo, variamos un poco la espera.
        a_station->fuel -= 10; 
        
        // Control de subdesbordamiento de combustible
        if (a_station->fuel < 0) {
            a_station->fuel = 0;
        }

        if(a_station->fuel >= 0 && a_station->fuel < 40){
            send_warning_message_servidor(a_station);
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
    send_warning_message_servidor(a_station);
    
    return NULL;  
}

/**
 * @brief Punto de entrada principal para el proceso de lanzamiento de la estación.
 */
int main(int argc, char *argv[]) {

    srand((unsigned int)(time(NULL) ^ getpid()));

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

    conectar_ipc();

    msg_communication_initialization init_msg;
    init_msg.typeStored = STATION;
    init_msg.a_station = my_station; 
    if (mq_send(cola_inicializacion, (const char*)&init_msg, sizeof(init_msg), 0) == -1) {
        perror("Failed to send initialization message to server");
        exit(1);
    }
    // Lanzamiento y gestión del hilo operativo de la estación
    if (pthread_create(&thread_station, NULL, run_station, &my_station) != 0) {
        perror("Error creating thread station");
        exit(1);
    }
    
    // Bloquea el proceso principal hasta que la estación termine sus operaciones
    pthread_join(thread_station, NULL);

    if (cola_aviso_poco_combustible != (mqd_t)-1) {
        mq_close(cola_aviso_poco_combustible);
    }
    if (cola_inicializacion != (mqd_t)-1) {
        mq_close(cola_inicializacion);
    }

    printf("\nFinalizando proceso.\n");
    return 0;
}