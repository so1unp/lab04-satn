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
    int station_pid;
    int pos_x_station;
    int pos_y_station;
    int fuel_left;
} msg_communication_station_warning;

typedef struct {
    EntityType typeStored;
    t_nave ship;
    //STATION
} msg_communication_initialization;

#endif 