#include <stdio.h>
#include <unistd.h>
#include "estacion.h"
#include <signal.h>

void inicializar_estacion(station *a_station, int pos_x, int pos_y) {
    if (a_station == NULL)
        return;

    a_station->pos_x = pos_x;
    a_station->pos_y = pos_y;
    a_station->fuel = 100; 
    a_station->fuelWarningSent = false;
    a_station->isDestroyed = false;
}

void dar_aviso_naves(station *a_station) {
    
    if (a_station == NULL)
        return;
    if (a_station->fuelWarningSent)
        return;
    
    // Mandar un mensaje a todas las naves del servidor
}
