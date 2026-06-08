#ifndef NAVE_H
#define NAVE_H

#include <pthread.h>

typedef enum {
    ESTADO_VIVO,
    ESTADO_GAMEOVER_OXIGENO,
    ESTADO_GAMEOVER_COMBUSTIBLE
} e_estado_nave;

// Inventario con los materiales reales de la consigna
typedef struct {
    int deuterio;    // Combustible
    int mutexio;     // Oxígeno / Usos múltiples (Común)
    int semaforita;  // Oxígeno / Usos múltiples (Raro)
    int kernelio;    // Oxígeno / Usos múltiples (Valioso)
} t_inventario;

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

void inicializar_nave(t_nave* nave, int x_inicial, int y_inicial);
void destruir_nave(t_nave* nave);

#endif // NAVE_H