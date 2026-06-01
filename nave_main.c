#include <stdio.h>
#include "nave.h"

// Función auxiliar para mostrar el estado actual de la nave 
void mostrar_estado_nave(t_nave* nave) {
    // Siempre bloqueamos el mutex antes de leer la estructura si estamos en un entorno multihilo
    pthread_mutex_lock(&nave->mutex_nave);

    printf("\n===  ESTADO DE LA NAVE ===");
    printf("\n• Posición     : (%d, %d)", nave->pos_x, nave->pos_y);
    printf("\n• Oxígeno      : %d%%", nave->oxigeno);
    printf("\n• Combustible  : %d%%", nave->combustible);
    
    printf("\n• Estado       : ");
    switch(nave->estado) {
        case ESTADO_VIVO: printf("VIVO (Operacional)"); break;
        case ESTADO_GAMEOVER_OXIGENO: printf("GAME OVER (Sin Oxígeno)"); break;
        case ESTADO_GAMEOVER_COMBUSTIBLE: printf("GAME OVER (Sin Combustible)"); break;
    }

    printf("\n• Inventario   :");
    printf("\n  - Pepenio    : %d unidades (Materia prima Oxígeno)", nave->inventario.pepenio);
    printf("\n  - Roberterio : %d unidades (Materia prima Combustible)", nave->inventario.roberterio);
    printf("\n=============================\n");

    pthread_mutex_unlock(&nave->mutex_nave);
}

int main() {
    t_nave mi_nave;

    // 1. Inicialización
    inicializar_nave(&mi_nave, 5, 10);
    printf("--- Nave recién salida del hangar ---");
    mostrar_estado_nave(&mi_nave);

    // 2. Simulación de acciones de hilos (Modificaciones seguras usando el Mutex)
    printf("\n[Simulación] La nave viaja a un asteroide y mina recursos...\n");
    
    pthread_mutex_lock(&mi_nave.mutex_nave);
    
    // Simula gasto de viaje
    mi_nave.pos_x = 12;
    mi_nave.pos_y = 45;
    mi_nave.combustible -= 15; // Gastó combustible por propulsión
    mi_nave.oxigeno -= 5;       // Pasó el tiempo (Soporte vital)

    // Simula extracción exitosa en el asteroide
    mi_nave.inventario.pepenio += 25;     
    mi_nave.inventario.roberterio += 40;  
    
    pthread_mutex_unlock(&mi_nave.mutex_nave);

    // 3. Mostrar estado post-minería
    mostrar_estado_nave(&mi_nave);

    // 4. Limpieza del sistema
    destruir_nave(&mi_nave);
    printf("\nRecursos del sistema operativo (Mutex) liberados correctamente.\n");

    return 0;
}