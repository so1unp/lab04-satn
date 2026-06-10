#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <pthread.h>
#include <defaultconfig.h>
#include <semaphore.h> 

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

#endif 