#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <pthread.h>
#include <defaultconfig.h>
#include <semaphore.h> 
#include "map.h"

typedef struct {
    pid_t shipPid;
    int shipXMovement;
    int shipYMovement;
} msg_communication_movement;

typedef struct {
    pid_t shipPid;
    int asteroidXPosition;
    int asteroidYPosition;
    int mutexioExtracted;
    int semaforitaQuantity;
    int deuterioQuantity;
    int kernelioQuantity;
} msg_communication_extraction;

typedef struct {
    pid_t stationPid;
    int pos_x_station;
    int pos_y_station;
    int fuel_station;
} msg_communication_station_warning;

typedef struct {
    EntityType typeStored;
    t_nave ship;
    //STATION
} msg_communication_initialization;

#endif 