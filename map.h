#ifndef MAP_H
#define MAP_h

#include <asteroid.h>
#include <pthread.h>

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

//TO DELETE
#define MAP_WIDTH  0
#define MAP_HEIGHT  0
//TO DELETE

typedef struct {
    int height;
    int width;
    MapCell map[MAP_WIDTH][MAP_HEIGHT];
} Map;

#endif 