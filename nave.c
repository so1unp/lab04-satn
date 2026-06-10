#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mqueue.h>
#include "nave.h"
#include "map.h"

#define TECLA_SALIR 'q'
#define TECLA_MINAR 'm'

// Variables globales para IPC
Map *gameMap = NULL;
int sharedMemoryFd = -1;
mqd_t cola_movimiento = -1;
char mq_name[50];

// Visual local de la nave controlada
char caracter_nave = '^';
volatile int juego_ejecutando = 1;

// Instancia local para el control de recursos propios (Soporte Vital)
t_nave mi_nave;

// --- FUNCIONES DEL MODELO ---

void inicializar_nave(t_nave* nave, int x_inicial, int y_inicial) {
    if (nave == NULL) return;

    nave->id = getpid(); // Tu ID oficial es el PID del proceso
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
    pthread_mutex_destroy(&(nave->mutex_nave));
}

// --- CONEXION A LOS RECURSOS IPC DEL SERVIDOR ---

void conectar_ipc() {
    // 1. Conexión a la Memoria Compartida del Servidor (Solo lectura/escritura)
    sharedMemoryFd = shm_open(SHARED_MEMORY_PATH, O_RDWR, 0660);
    if (sharedMemoryFd < 0) {
        perror("Error en shm_open de la nave. ¿El servidor esta corriendo?");
        exit(1);
    }

    gameMap = (Map *)mmap(NULL, sizeof(Map), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);
    if (gameMap == MAP_FAILED) {
        perror("Error en mmap de la nave");
        exit(1);
    }

    // 2. Creación de la Cola de Mensajes única basada en el PID
    snprintf(mq_name, sizeof(mq_name), "/mq_nave_%d", getpid());
    
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(t_mensaje_movimiento);
    attr.mq_curmsgs = 0;

    // La nave crea la cola para que el servidor lea de ella
    cola_movimiento = mq_open(mq_name, O_CREAT | O_WRONLY, 0660, &attr);
    if (cola_movimiento == (mqd_t)-1) {
        perror("Error al crear la cola de mensajes de la nave");
        exit(1);
    }
}

void desconectar_ipc() {
    if (gameMap != NULL) {
        munmap(gameMap, sizeof(Map));
    }
    if (sharedMemoryFd >= 0) {
        close(sharedMemoryFd);
    }
    if (cola_movimiento != (mqd_t)-1) {
        mq_close(cola_movimiento);
        mq_unlink(mq_name);
    }
}

// --- FUNCIONES VISUALES (Ncurses basados en Memoria Compartida) ---

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

void renderizar_universo_shm(t_nave* nave) {
    int alto_pantalla, ancho_pantalla;
    getmaxyx(stdscr, alto_pantalla, ancho_pantalla); 

    int mapa_start_y = (alto_pantalla / 2) - (DEFAULT_MAP_HEIGHT / 2);
    int mapa_start_x = (ancho_pantalla - DEFAULT_MAP_WIDTH) / 2;
    int panel_start_y = mapa_start_y + DEFAULT_MAP_HEIGHT + 1;
    int panel_start_x = (ancho_pantalla - 52) / 2;

    clear();

    // 1. RENDERIZAR MAPA LEYENDO LA MEMORIA COMPARTIDA (gameMap)
    for (int y = 0; y < DEFAULT_MAP_HEIGHT; y++) {
        for (int x = 0; x < DEFAULT_MAP_WIDTH; x++) {
            
            // Si la celda de la SHM contiene nuestra nave
            if (gameMap->map[x][y].typeStored == SHIP && gameMap->map[x][y].ship.id == nave->id) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)caracter_nave);
            } 
            // Si es la nave de otro jugador
            else if (gameMap->map[x][y].typeStored == SHIP) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)'S');
            }
            // Si es un asteroide colocado por el Servidor
            else if (gameMap->map[x][y].typeStored == ASTEROID) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)'A');
            }
            // Si es una estación
            else if (gameMap->map[x][y].typeStored == STATION) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)'E');
            }
            // Espacio vacío o límites del mapa locales
            else {
                if (y == 0 || y == DEFAULT_MAP_HEIGHT - 1 || x == 0 || x == DEFAULT_MAP_WIDTH - 1) {
                    mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)'#');
                } else {
                    mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)' ');
                }
            }
        }
    }

    // 2. RENDERIZAR PANEL DE CONTROL LOCAL
    pthread_mutex_lock(&nave->mutex_nave);
    mvprintw(panel_start_y,     panel_start_x, " ================================================== ");
    mvprintw(panel_start_y + 1, panel_start_x, "   PID NAVE: %d | OXIGENO: %3d%% | NAFTA: %3d%%", 
             nave->id, nave->oxigeno, nave->combustible);
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
    pthread_mutex_unlock(&nave->mutex_nave);
    
    refresh();
}

// --- HILOS CONCURRENTES ---

void* hilo_renderizador(void* arg) {
    t_nave* nave_compartida = (t_nave*)arg;
    while (juego_ejecutando) {
        renderizar_universo_shm(nave_compartida);
        usleep(50000); 
    }
    return NULL;
}

void* hilo_soporte_vital(void* arg) {
    t_nave* nave_compartida = (t_nave*)arg;
    while (juego_ejecutando) {
        sleep(1); 
        pthread_mutex_lock(&nave_compartida->mutex_nave);
        if (nave_compartida->estado == ESTADO_VIVO) {
            nave_compartida->oxigeno -= 1;
            if (nave_compartida->oxigeno <= 0) {
                nave_compartida->oxigeno = 0;
                nave_compartida->estado = ESTADO_GAMEOVER_OXIGENO;
            }
        }
        pthread_mutex_unlock(&nave_compartida->mutex_nave);
    }
    return NULL;
}

// --- ENVIAR COMANDO AL SERVIDOR ---

void enviar_comando(char cmd) {
    t_mensaje_movimiento msg;
    msg.id_nave = getpid();
    msg.comando = cmd;

    // Se envía el mensaje de forma no bloqueante a la cola del servidor
    if (mq_send(cola_movimiento, (const char*)&msg, sizeof(msg), 0) == -1) {
        // Si falla porque el servidor no lee, evitamos romper el flujo gráfico
    }
}

// --- PROGRAMA PRINCIPAL ---

int main() {
    int ch;
    pthread_t thread_visual, thread_oxigeno;

    // Conexión obligatoria al entorno IPC antes de inicializar ncurses
    conectar_ipc();

    // Seteamos la nave local con su PID y posición tentativa
    inicializar_nave(&mi_nave, 5, 5);
    inicializar_visuales();

    pthread_create(&thread_visual, NULL, hilo_renderizador, (void*)&mi_nave);
    pthread_create(&thread_oxigeno, NULL, hilo_soporte_vital, (void*)&mi_nave);

    // Bucle de Input: Lee teclas y las despacha por la Cola de Mensajes
    while ((ch = getch()) != TECLA_SALIR) {
        
        pthread_mutex_lock(&mi_nave.mutex_nave);
        if (mi_nave.estado == ESTADO_VIVO && mi_nave.combustible > 0) {
            
            switch (ch) {
                case KEY_UP:
                    caracter_nave = '^';
                    mi_nave.combustible -= 2;
                    enviar_comando('U');
                    break;
                case KEY_DOWN:
                    caracter_nave = 'v';
                    mi_nave.combustible -= 2;
                    enviar_comando('D');
                    break;
                case KEY_LEFT:
                    caracter_nave = '<';
                    mi_nave.combustible -= 2;
                    enviar_comando('L');
                    break;
                case KEY_RIGHT:
                    caracter_nave = '>';
                    mi_nave.combustible -= 2;
                    enviar_comando('R');
                    break;
                case TECLA_MINAR:
                case 'M': 
                    mi_nave.combustible -= 5;
                    enviar_comando('M');
                    break;
            }
        }
        pthread_mutex_unlock(&mi_nave.mutex_nave);
    }

    // Apagado y limpieza total de recursos del Sistema Operativo
    juego_ejecutando = 0;           
    pthread_join(thread_visual, NULL); 
    pthread_join(thread_oxigeno, NULL); 

    finalizar_visuales();
    destruir_nave(&mi_nave);
    desconectar_ipc();

    return 0;
}