#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <pthread.h>
#include <defaultconfig.h>
#include <semaphore.h> 
#include <nave.h>
#include <estacion.h>

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
    station a_station;
} msg_communication_initialization;

typedef struct {
    pid_t shipPid;
} msg_communication_logout;

//Estructuras creadas para el trading

typedef struct {
    pid_t ship_pid;
    TradeType type;
    int deuterio_to_sell;
    int mutexio_to_sell;
    int semaforita_to_sell;
    int kernelio_to_sell;
} msg_trade_request;

typedef struct {
    bool success;
    int fuel_refueled;
    int oxygen_refueled;
    char message[50];
} msg_trade_response;

#endif