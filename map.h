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
    union {
        //SHIP
        Asteroid asteroid;
        //STATION
    } entities;
} MapCell;


//TO DELETE
#define MAP_WIDTH  0
#define MAP_HEIGHT  0
//TO DELETE

typedef struct {
    int height;
    int width;
    pthread_mutex_t mutex_cells[MAP_WIDTH][MAP_HEIGHT];
    MapCell map[MAP_WIDTH][MAP_HEIGHT];
} Map;


#endif 