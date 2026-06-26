#ifndef MAP_H
#define MAP_H

#include <nave.h>
#include <estacion.h>
#include <asteroid.h>
#include <pthread.h>
#include <defaultconfig.h>
#include <semaphore.h> 

typedef struct {
    EntityType typeStored;
    t_nave ship;
    station a_station;
    Asteroid asteroid;
    sem_t mutex;
} MapCell;

typedef struct {
    MapCell map[DEFAULT_MAP_HEIGHT][DEFAULT_MAP_WIDTH];
} Map;


#endif 