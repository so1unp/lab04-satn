#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include "nave.h"

#define TECLA_SALIR 'q'
#define TECLA_MINAR 'm'

// --- FUNCIONES DEL MODELO ---

void inicializar_nave(t_nave* nave, int x_inicial, int y_inicial) {
    if (nave == NULL) return;

    nave->id = -1; // Seteo por defecto para el modo test / handshake
    nave->pos_x = x_inicial;
    nave->pos_y = y_inicial;
    nave->oxigeno = 100;       
    nave->combustible = 100;   
    nave->estado = ESTADO_VIVO;
    nave->inventario.pepenio = 0;
    nave->inventario.roberterio = 0;

    if (pthread_mutex_init(&(nave->mutex_nave), NULL) != 0) {
        perror("Error al inicializar el mutex de la nave");
    }
}

void destruir_nave(t_nave* nave) {
    if (nave == NULL) return;

    if (pthread_mutex_destroy(&(nave->mutex_nave)) != 0) {
        perror("Error al destruir el mutex de la nave");
    }
}

// --- FUNCIONES VISUALES (Ncurses) ---

void inicializar_visuales() {
    initscr();             
    cbreak();              
    noecho();              
    keypad(stdscr, TRUE);  
    curs_set(0);           
    refresh();
}

void finalizar_visuales() {
    endwin();              
}

void renderizar_estado_nave_ncurses(t_nave* nave) {
    int alto, ancho;
    getmaxyx(stdscr, alto, ancho); 

    pthread_mutex_lock(&nave->mutex_nave);

    int start_y = (alto / 2) - 7;
    int start_x = (ancho - 50) / 2;

    clear();

    mvprintw(start_y,     start_x, " ========================================== ");
    mvprintw(start_y + 1, start_x, "         PANEL DE CONTROL - INTERACTIVO        ");
    mvprintw(start_y + 2, start_x, " ========================================== ");
    
    if (nave->id == -1) {
        mvprintw(start_y + 4, start_x, "  • ID de Nave    : [Modo Test Interactivo]");
    } else {
        mvprintw(start_y + 4, start_x, "  • ID de Nave    : #%d", nave->id);
    }

    mvprintw(start_y + 5, start_x, "  • Posición      : (%d, %d)", nave->pos_x, nave->pos_y);
    mvprintw(start_y + 6, start_x, "  • Oxígeno       : %d%%", nave->oxigeno);
    mvprintw(start_y + 7, start_x, "  • Combustible   : %d%%", nave->combustible);
    
    mvprintw(start_y + 8, start_x, "  • Estado        : ");
    switch(nave->estado) {
        case ESTADO_VIVO: 
            printw("VIVO (Operacional)"); 
            break;
        case ESTADO_GAMEOVER_OXIGENO: 
            printw("GAME OVER (Asfixia en el espacio)"); 
            break;
        case ESTADO_GAMEOVER_COMBUSTIBLE: 
            printw("GAME OVER (Deriva espacial sin Nafta)"); 
            break;
    }

    mvprintw(start_y + 10, start_x, " INVENTARIO:");
    mvprintw(start_y + 11, start_x, "  [-] Pepenio    : %d uds. (Soporte Vital)", nave->inventario.pepenio);
    mvprintw(start_y + 12, start_x, "  [-] Roberterio : %d uds. (Propulsión)", nave->inventario.roberterio);
    
    mvprintw(start_y + 14, start_x, " ========================================== ");
    mvprintw(start_y + 15, start_x, " [Flechas] Moverse | [M] Minar Recursos | [Q] Salir");

    pthread_mutex_unlock(&nave->mutex_nave);
    refresh();
}



int main() {
    t_nave mi_nave;
    int ch;

    // Inicializamos estructuras y entorno gráfico
    inicializar_nave(&mi_nave, 5, 10);
    inicializar_visuales();

    // Renderizado inicial antes de capturar comandos
    renderizar_estado_nave_ncurses(&mi_nave);

    // Ciclo de juego interactivo
    while ((ch = getch()) != TECLA_SALIR) {
        
        pthread_mutex_lock(&mi_nave.mutex_nave);

        if (mi_nave.estado == ESTADO_VIVO) {
            switch (ch) {
                case KEY_UP:
                    mi_nave.pos_y--;
                    mi_nave.combustible -= 2;
                    mi_nave.oxigeno -= 1;
                    break;
                case KEY_DOWN:
                    mi_nave.pos_y++;
                    mi_nave.combustible -= 2;
                    mi_nave.oxigeno -= 1;
                    break;
                case KEY_LEFT:
                    mi_nave.pos_x--;
                    mi_nave.combustible -= 2;
                    mi_nave.oxigeno -= 1;
                    break;
                case KEY_RIGHT:
                    mi_nave.pos_x++;
                    mi_nave.combustible -= 2;
                    mi_nave.oxigeno -= 1;
                    break;

                case TECLA_MINAR:
                case 'M': 
                    mi_nave.inventario.pepenio += 5;
                    mi_nave.inventario.roberterio += 10;
                    mi_nave.combustible -= 5; 
                    break;
                
                default:
                    break;
            }

            if (mi_nave.oxigeno <= 0) {
                mi_nave.oxigeno = 0;
                mi_nave.estado = ESTADO_GAMEOVER_OXIGENO;
            }
            if (mi_nave.combustible <= 0) {
                mi_nave.combustible = 0;
                mi_nave.estado = ESTADO_GAMEOVER_COMBUSTIBLE;
            }
        }
        
        pthread_mutex_unlock(&mi_nave.mutex_nave);
        renderizar_estado_nave_ncurses(&mi_nave);
    }

    finalizar_visuales();
    destruir_nave(&mi_nave);

    printf("Prueba interactiva finalizada con éxito.\n");
    return 0;
}