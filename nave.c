#include "nave.h"
#include <stdio.h>
#include <stdlib.h>

void inicializar_nave(t_nave* nave, int x_inicial, int y_inicial) {
    if (nave == NULL) return;

    // Valores iniciales de la simulación
    nave->pos_x = x_inicial;
    nave->pos_y = y_inicial;
    nave->oxigeno = 100;       // Arranca al 100%
    nave->combustible = 100;   // Arranca al 100%
    nave->estado = ESTADO_VIVO;

    // Inventario vacío al empezar
    nave->inventario.pepenio = 0;
    nave->inventario.roberterio = 0;

    // Inicializamos el mutex de POSIX
    if (pthread_mutex_init(&(nave->mutex_nave), NULL) != 0) {
        perror("Error al inicializar el mutex de la nave");
    }
}

void destruir_nave(t_nave* nave) {
    if (nave == NULL) return;

    // Destruimos el mutex para liberar los recursos del sistema operativo
    if (pthread_mutex_destroy(&(nave->mutex_nave)) != 0) {
        perror("Error al destruir el mutex de la nave");
    }
}