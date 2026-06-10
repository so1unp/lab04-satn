#ifndef NAVE_H
#define NAVE_H

#include <pthread.h>

typedef enum {
    ESTADO_VIVO,
    ESTADO_GAMEOVER_OXIGENO,
    ESTADO_GAMEOVER_COMBUSTIBLE
} e_estado_nave;

typedef struct {
    int deuterio;    
    int mutexio;     
    int semaforita;  
    int kernelio;    
} t_inventario;

typedef struct {
    int id; // PID
    int pos_x;
    int pos_y;
    int oxigeno;
    int combustible;
    e_estado_nave estado;
    t_inventario inventario;
    pthread_mutex_t mutex_nave;
} t_nave;

// Estructura para enviar comandos de movimiento por la Cola de Mensajes
typedef struct {
    int id_nave;
    char comando; // 'U'=Up, 'D'=Down, 'L'=Left, 'R'=Right, 'M'=Mine
} t_mensaje_movimiento;

void inicializar_nave(t_nave* nave, int x_inicial, int y_inicial);
void destruir_nave(t_nave* nave);

#endif 