#ifndef ASTEROIDE_H
#define ASTEROIDE_H

#include <stdbool.h>
#include <semaphore.h> 

typedef struct {
    int deuterio;
    int mutexio;
    int semaforita;
    int kernelio;
    bool isActive;
    sem_t mutex;
} Asteroid;

#endif