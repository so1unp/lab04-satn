#ifndef STATION_H
#define STATION_H

#include <stdlib.h>
#include <stdbool.h>

#define MAX_HANGAR_CAPACITY 3

typedef struct
{
    char name[30];
    int pos_x;
    int pos_y;

    // Control de Hangar y Señales
    int hangarCapacity;
    pid_t shipPids[MAX_HANGAR_CAPACITY]; // Arreglo para guardar los PID de las naves
    
    // Combustible
    int fuelCapacity;
    int currentFuel;

    // Estado de la Estación
    bool fuelWarningSent;  // true si ya notificó a las naves
    bool isDestroyed;      // true si se quedó sin combustible y explotó

} station;

#endif