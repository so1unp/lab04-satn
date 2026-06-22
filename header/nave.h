#ifndef NAVE_H
#define NAVE_H

#include <pthread.h>
#include <semaphore.h> 

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
    sem_t mutex;
} t_nave;

void inicializar_nave(t_nave* nave);
void destruir_nave(t_nave* nave);

#endif 