/**
 * @file estacion.h
 * @brief Definición de estructuras y prototipos de funciones para la gestión de estaciones espaciales.
 */

#ifndef ESTACION_H
#define ESTACION_H

#include <stdbool.h>
#include <sys/types.h>
#include <semaphore.h> 

/* --- Constantes del Sistema --- */
#define MAX_HANGAR_CAPACITY 3
#define CANT_ARGUMENTS 3
#define TIME_OF_CONSUME_FUEL 3

/* --- Estructuras de Datos --- */

/**
 * @struct station
 * @brief Representa el estado interno, recursos y localización de una estación espacial.
 */
typedef struct {
    char name[30];                       /**< Nombre identificatorio de la estación */
    int pos_x;                           /**< Coordenada en el eje X dentro del mapa espacial */
    int pos_y;                           /**< Coordenada en el eje Y dentro del mapa espacial */
    
    pid_t shipPids[MAX_HANGAR_CAPACITY]; /**< Listado de PIDs de las naves actualmente atracadas en el hangar */
    
    int fuel;                            /**< Nivel actual de combustible (porcentaje o unidades) */

    bool fuelWarningSent;                /**< Indicador de si ya se envió la alerta de bajo combustible */
    bool isDestroyed;                    /**< Estado vital de la estación (true si colapsó o fue destruida) */
    bool ship_message;                    

    sem_t hangar_sem; /**< Semáforo contador: limita a MAX_HANGAR_CAPACITY naves simultáneas */
} station;

/* --- Prototipos de Funciones --- */

/**
 * @brief Inicializa los valores por defecto de la estructura de la estación.
 */
void inicializar_estacion(station *a_station, int pos_x, int pos_y);

/**
 * @brief Envía una alerta con el estado actual de la estación hacia la cola de mensajes del servidor.
 */
void send_message_servidor(station *a_station);

/**
 * @brief Simula el consumo progresivo de combustible de la estación a lo largo del tiempo.
 */
void consume_fuel(station *a_station);

#endif /* ESTACION_H */