#ifndef NAVE_H
#define NAVE_H

#include <pthread.h>

// Definición de los estados posibles de la nave
typedef enum {
    ESTADO_VIVO,
    ESTADO_GAMEOVER_OXIGENO,
    ESTADO_GAMEOVER_COMBUSTIBLE
} e_estado_nave;

// Estructura para el inventario de recursos recolectados
typedef struct {
    int pepenio;     // Mineral para fabricación de oxígeno
    int roberterio;  // Mineral para fabricación de combustible
} t_inventario;

// Estructura principal de la Nave
typedef struct {
    int id;            
    int pos_x;
    int pos_y;
    int oxigeno;
    int combustible;
    e_estado_nave estado;
    t_inventario inventario;
    pthread_mutex_t mutex_nave;
} t_nave;

// Promesas de funciones (Interfaz pública)
void inicializar_nave(t_nave* nave, int x_inicial, int y_inicial);
void destruir_nave(t_nave* nave);

#endif 