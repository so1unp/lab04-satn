#ifndef MAP_H
#define MAP_h

#include <nave.h>
#include <asteroid.h>
#include <pthread.h>
#include <defaultconfig.h>
#include <semaphore.h> 

typedef enum {
    EMPTY,
    SHIP,
    ASTEROID,
    STATION
} EntityType;

typedef struct {
    EntityType typeStored;
    t_nave ship;
    Asteroid asteroid;
    //STATION
    sem_t mutex;
} MapCell;


typedef struct {
    MapCell map[DEFAULT_MAP_WIDTH][DEFAULT_MAP_HEIGHT];
} Map;


#endif 