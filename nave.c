#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include "nave.h"

#define TECLA_SALIR 'q'
#define TECLA_MINAR 'm'

// Dimensiones del mapa del juego
#define MAPA_ALTO 15
#define MAPA_ANCHO 50

// Matriz global que simula el espacio
char mapa_espacial[MAPA_ALTO][MAPA_ANCHO];

// Variable para guardar el caracter visual de la nave segun su direccion
char caracter_nave = '^';

// --- FUNCIONES DEL MODELO ---

void inicializar_nave(t_nave* nave, int x_inicial, int y_inicial) {
    if (nave == NULL) return;

    nave->id = -1; 
    nave->pos_x = x_inicial;
    nave->pos_y = y_inicial;
    nave->oxigeno = 100;       
    nave->combustible = 100;   
    nave->estado = ESTADO_VIVO;
    
    nave->inventario.deuterio = 0;
    nave->inventario.mutexio = 0;
    nave->inventario.semaforita = 0;
    nave->inventario.kernelio = 0;

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

// Genera un mapa de prueba con asteroides (A) y estaciones (E)
void generar_mapa_local() {
    for (int y = 0; y < MAPA_ALTO; y++) {
        for (int x = 0; x < MAPA_ANCHO; x++) {
            if (y == 0 || y == MAPA_ALTO - 1 || x == 0 || x == MAPA_ANCHO - 1) {
                mapa_espacial[y][x] = '#';
            } else {
                mapa_espacial[y][x] = ' '; 
            }
        }
    }
    // Colocamos algunos asteroides y una estacion fijos
    mapa_espacial[3][15] = 'A';
    mapa_espacial[4][16] = 'A';
    mapa_espacial[10][8]  = 'A';
    mapa_espacial[7][35] = 'E'; 
}

// Funcion auxiliar para verificar si hay un objetivo cerca de la nave
// Retorna 1 si encuentra el caracter buscado en las 4 direcciones adyacentes
int verificar_proximidad(int nave_x, int nave_y, char objetivo) {
    if (mapa_espacial[nave_y - 1][nave_x] == objetivo) return 1; // Arriba
    if (mapa_espacial[nave_y + 1][nave_x] == objetivo) return 1; // Abajo
    if (mapa_espacial[nave_y][nave_x - 1] == objetivo) return 1; // Izquierda
    if (mapa_espacial[nave_y][nave_x + 1] == objetivo) return 1; // Derecha
    return 0;
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

void renderizar_todo_ncurses(t_nave* nave) {
    int alto_pantalla, ancho_pantalla;
    getmaxyx(stdscr, alto_pantalla, ancho_pantalla); 

    pthread_mutex_lock(&nave->mutex_nave);

    int mapa_start_y = (alto_pantalla / 2) - 10;
    int mapa_start_x = (ancho_pantalla - MAPA_ANCHO) / 2;
    int panel_start_y = mapa_start_y + MAPA_ALTO + 1;
    int panel_start_x = (ancho_pantalla - 52) / 2;

    clear();

    // 1. RENDERIZAR EL MAPA ESPACIAL
    for (int y = 0; y < MAPA_ALTO; y++) {
        for (int x = 0; x < MAPA_ANCHO; x++) {
            if (y == nave->pos_y && x == nave->pos_x) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)caracter_nave);
            } else {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)mapa_espacial[y][x]);
            }
        }
    }

    // 2. RENDERIZAR PANEL DE CONTROL
    mvprintw(panel_start_y,     panel_start_x, " ================================================== ");
    mvprintw(panel_start_y + 1, panel_start_x, "   POSICION: (%2d, %2d) | OXIGENO: %3d%% | NAFTA: %3d%%", 
             nave->pos_x, nave->pos_y, nave->oxigeno, nave->combustible);
    mvprintw(panel_start_y + 2, panel_start_x, "   ESTADO  : ");
    
    switch(nave->estado) {
        case ESTADO_VIVO:                  printw("OPERACIONAL"); break;
        case ESTADO_GAMEOVER_OXIGENO:      printw("GAME OVER (Sin Oxigeno)"); break;
        case ESTADO_GAMEOVER_COMBUSTIBLE:  printw("GAME OVER (Sin Combustible)"); break;
    }

    mvprintw(panel_start_y + 4, panel_start_x, "   INVENTARIO: D: %d | M: %d | S: %d | K: %d",
             nave->inventario.deuterio, nave->inventario.mutexio, 
             nave->inventario.semaforita, nave->inventario.kernelio);
    mvprintw(panel_start_y + 5, panel_start_x, " ================================================== ");
    mvprintw(panel_start_y + 6, panel_start_x, " [Flechas] Volar | [M] Minar junto a Asteroide (A) | [Q] Salir");

    pthread_mutex_unlock(&nave->mutex_nave);
    refresh();
}

// --- PROGRAMA PRINCIPAL ---

int main() {
    t_nave mi_nave;
    int ch;

    generar_mapa_local();
    inicializar_nave(&mi_nave, 5, 5);
    inicializar_visuales();

    renderizar_todo_ncurses(&mi_nave);

    while ((ch = getch()) != TECLA_SALIR) {
        
        pthread_mutex_lock(&mi_nave.mutex_nave);

        if (mi_nave.estado == ESTADO_VIVO) {
            switch (ch) {
                case KEY_UP:
                    // Verifica que el casillero de arriba este vacio antes de avanzar
                    if (mi_nave.pos_y > 1 && mapa_espacial[mi_nave.pos_y - 1][mi_nave.pos_x] == ' ') {
                        mi_nave.pos_y--;
                        caracter_nave = '^';
                        mi_nave.combustible -= 2;
                        mi_nave.oxigeno -= 1;
                    }
                    break;
                case KEY_DOWN:
                    if (mi_nave.pos_y < MAPA_ALTO - 2 && mapa_espacial[mi_nave.pos_y + 1][mi_nave.pos_x] == ' ') {
                        mi_nave.pos_y++;
                        caracter_nave = 'v';
                        mi_nave.combustible -= 2;
                        mi_nave.oxigeno -= 1;
                    }
                    break;
                case KEY_LEFT:
                    if (mi_nave.pos_x > 1 && mapa_espacial[mi_nave.pos_y][mi_nave.pos_x - 1] == ' ') {
                        mi_nave.pos_x--;
                        caracter_nave = '<';
                        mi_nave.combustible -= 2;
                        mi_nave.oxigeno -= 1;
                    }
                    break;
                case KEY_RIGHT:
                    if (mi_nave.pos_x < MAPA_ANCHO - 2 && mapa_espacial[mi_nave.pos_y][mi_nave.pos_x + 1] == ' ') {
                        mi_nave.pos_x++;
                        caracter_nave = '>';
                        mi_nave.combustible -= 2;
                        mi_nave.oxigeno -= 1;
                    }
                    break;

                case TECLA_MINAR:
                case 'M': 
                    // Logica corregida: Mina si tiene un asteroide 'A' al lado
                    if (verificar_proximidad(mi_nave.pos_x, mi_nave.pos_y, 'A')) {
                        mi_nave.combustible -= 5; 
                        mi_nave.inventario.deuterio += 12;
                        mi_nave.inventario.mutexio += 10;    
                        mi_nave.inventario.semaforita += 5;   
                        mi_nave.inventario.kernelio += 1;     
                    }
                    break;
                
                default:
                    break;
            }

            if (mi_nave.oxigeno <= 0) { mi_nave.oxigeno = 0; mi_nave.estado = ESTADO_GAMEOVER_OXIGENO; }
            if (mi_nave.combustible <= 0) { mi_nave.combustible = 0; mi_nave.estado = ESTADO_GAMEOVER_COMBUSTIBLE; }
        }
        
        pthread_mutex_unlock(&mi_nave.mutex_nave);
        renderizar_todo_ncurses(&mi_nave);
    }

    finalizar_visuales();
    destruir_nave(&mi_nave);

    printf("Simulacion grafica del espacio finalizada con exito.\n");
    return 0;
}