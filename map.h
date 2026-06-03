#ifndef MAP_H
#define MAP_h

#include <asteroid.h>
#include <pthread.h>
#include <defaultconfig.h>

typedef enum {
    EMPTY,
    SHIP,
    ASTEROID,
    STATION
} EntityType;

typedef struct {
    EntityType typeStored;
    //SHIP
    Asteroid asteroid;
    //STATION
    pthread_mutex_t mutex;
} MapCell;


typedef struct {
    int height;
    int width;
    MapCell map[DEFAULT_MAP_WIDTH][DEFAULT_MAP_HEIGHT];
} Map;


#endif 