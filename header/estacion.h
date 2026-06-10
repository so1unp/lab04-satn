#ifndef ESTACION_H
#define ESTACION_H

#include <stdlib.h>
#include <stdbool.h>

#define MAX_HANGAR_CAPACITY 3
// #define MAX_FUEL_CAPACITY 100

typedef struct {
    char name[30];
    int pos_x;
    int pos_y;
    // Control de Hangar y Señales
    // Deberia tener la referencia a la cola de mensajes de las naves.
    // avisosnaves
    
    pid_t shipPids[MAX_HANGAR_CAPACITY]; // Arreglo para guardar los PID de las naves que estan en mi estacion
    
    // Combustible
    int fuel;

    // Estado de la Estación
    bool fuelWarningSent;  // true si ya notificó a las naves
    bool isDestroyed;      // true si se quedó sin combustible y explotó

} station;

void inicializar_estacion(station* a_station, int pos_x, int pos_y);
void dar_aviso_naves(station* a_station);

#endif