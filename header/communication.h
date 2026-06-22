#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <pthread.h>
#include <defaultconfig.h>
#include <semaphore.h> 
#include <nave.h>

typedef struct {
    pid_t shipPid;
    int shipCurrentX;
    int shipCurrentY;
    int shipXMovement;
    int shipYMovement;
} msg_communication_movement;

typedef struct {
    pid_t shipPid;
    int shipCurrentX;
    int shipCurrentY;
    int asteroidXPosition;
    int asteroidYPosition;
    int mutexioQuantity;
    int semaforitaQuantity;
    int deuterioQuantity;
    int kernelioQuantity;
} msg_communication_extraction;

typedef struct {
    pid_t stationPid;      // <-- Agregamos el PID para identificar el proceso
    int pos_x_station;
    int pos_y_station;
    int fuel_station;      // <-- Cambiado de fuel_left a fuel_station
} msg_communication_station_warning;

typedef struct {
    EntityType typeStored;
    t_nave ship;
    //STATION
} msg_communication_initialization;

// Mensaje que envía la Nave a la Estación para solicitar el refuel
typedef struct {
    pid_t shipPid;
    int recursosEntregados; // Cantidad total de minerales que regala a la estación
} msg_intercambio_solicitud;

// Respuesta que la Estación le devuelve a la Nave
typedef struct {
    bool intercambioExitoso;
    int nuevoCombustibleNave;
    int nuevoOxigenoNave;
} msg_intercambio_respuesta;

#endif 