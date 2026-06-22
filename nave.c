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
#include "communication.h"

#define TECLA_SALIR 'q'
#define TECLA_MINAR 'm'
#define DURACION_ALERTA_SEGUNDOS 3

// === NUEVO: DEFINICIÓN DE TECLA DE INTERCAMBIO ===
#define TECLA_ESTACION 'e'
// ======================================================

void* hilo_renderizador(void* arg);
void* hilo_soporte_vital(void* arg);
void* hilo_aviso_estacion();
int obtener_posicion_shm(int *actualX, int *actualY);
int buscar_entidad_adyacente(int shipX, int shipY, EntityType objetivo, int *targetX, int *targetY);

// === NUEVO: PROTOTIPO DE LA FUNCIÓN DE INTERCAMBIO ===
void interactuar_con_estacion();
// ==========================================================

// Variables globales para IPC
Map *gameMap = NULL;
int sharedMemoryFd = -1;
mqd_t cola_aviso_estacion = -1;
mqd_t cola_inicializacion = -1;
mqd_t cola_movimiento = -1;
mqd_t cola_extraccion = -1;
char mq_name[50];

// Visual local de la nave controlada
char caracter_nave = '^';
volatile int juego_ejecutando = 1;
char mensaje_alerta[100] = "";
time_t timestamp_alerta = 0;

// Instancia local para el control de recursos propios (Soporte Vital)
t_nave mi_nave;

// Mutex usado para concurrencia local
pthread_mutex_t mutex_nave;

// --- FUNCIONES DEL MODELO ---

void inicializar_nave(t_nave* nave) {
    if (nave == NULL) return;

    nave->id = getpid(); // Tu ID oficial es el PID del proceso
    nave->oxigeno = 100;       
    nave->combustible = 100;   
    nave->estado = ESTADO_VIVO;
    
    nave->inventario.deuterio = 0;
    nave->inventario.mutexio = 0;
    nave->inventario.semaforita = 0;
    nave->inventario.kernelio = 0;
    if (pthread_mutex_init(&(mutex_nave), NULL) != 0) {
        perror("Error al inicializar el mutex de la nave");
    }
}

void destruir_nave(t_nave* nave) {
    if (nave == NULL) return;
    pthread_mutex_destroy(&(mutex_nave));
}

// --- CONEXION A LOS RECURSOS IPC DEL SERVIDOR ---

void conectar_ipc() {
    // 1. Conexión a la Memoria Compartida del Servidor (Solo lectura/escritura)
    sharedMemoryFd = shm_open(SHARED_MEMORY_PATH, O_RDWR, PERMISSIONS);
    if (sharedMemoryFd < 0) {
        perror("Error en shm_open de la nave. ¿El servidor esta corriendo?");
        exit(1);
    }

    gameMap = (Map *)mmap(NULL, sizeof(Map), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);
    if (gameMap == MAP_FAILED) {
        perror("Error en mmap de la nave");
        exit(1);
    }

    cola_inicializacion = mq_open(SERVER_CLIENT_INITIALIZATION_QUEUE_PATH, O_WRONLY);
    if (cola_inicializacion == (mqd_t)-1) {
        perror("Error abriendo cola de inicializacion");
        exit(1);
    }

    cola_movimiento = mq_open(SERVER_MOVEMENT_COMMUNICATION_QUEUE_PATH, O_WRONLY);
    if (cola_movimiento == (mqd_t)-1) {
        perror("Error abriendo cola de movimiento");
        exit(1);
    }

    cola_extraccion = mq_open(SERVER_EXTRACTION_COMMUNICATION_QUEUE_PATH, O_WRONLY);
    if (cola_extraccion == (mqd_t)-1) {
        perror("Error abriendo cola de extraccion");
        exit(1);
    }

    // 2. Creación de la Cola de Mensajes única basada en el PID
    snprintf(mq_name, sizeof(mq_name), "/mq_nave_%d", getpid());
    
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    
    // === NUEVO: TAMAÑO DE COLA ADAPTADO ===
    // Aseguramos que la cola local pueda albergar tanto alertas de estación como las respuestas de intercambio
    size_t max_struct_size = sizeof(msg_communication_station_warning);
    if (sizeof(msg_intercambio_respuesta) > max_struct_size) {
        max_struct_size = sizeof(msg_intercambio_respuesta);
    }
    attr.mq_msgsize = max_struct_size;
    // ===========================================
    
    attr.mq_curmsgs = 0;

    // La nave crea la cola para que el servidor le mande avisos (y ahora las estaciones respondan trueques)
    cola_aviso_estacion = mq_open(mq_name, O_CREAT | O_RDONLY, 0660, &attr);
    if (cola_aviso_estacion == (mqd_t)-1) {
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
    
    if (cola_aviso_estacion != (mqd_t)-1) {
        mq_close(cola_aviso_estacion);
        mq_unlink(mq_name);
    }
    if (cola_inicializacion != (mqd_t)-1) {
        mq_close(cola_inicializacion);
    }

    if (cola_movimiento != (mqd_t)-1) {
        mq_close(cola_movimiento);
    }

    if (cola_extraccion != (mqd_t)-1) {
        mq_close(cola_extraccion);
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
            
            if (gameMap->map[y][x].typeStored == SHIP && gameMap->map[y][x].ship.id == nave->id) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)caracter_nave);
                if (sem_wait(&gameMap->map[y][x].ship.mutex) == 0) {
                    pthread_mutex_lock(&mutex_nave);
                    nave->inventario = gameMap->map[y][x].ship.inventario;
                    pthread_mutex_unlock(&mutex_nave);
                    sem_post(&gameMap->map[y][x].ship.mutex);
                }
            } 
            else if (gameMap->map[y][x].typeStored == SHIP) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)'S');
            }
            else if (gameMap->map[y][x].typeStored == ASTEROID) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)'A');
            }
            else if (gameMap->map[y][x].typeStored == STATION) {
                mvaddch(mapa_start_y + y, mapa_start_x + x, (chtype)'E');
            }
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
    pthread_mutex_lock(&mutex_nave);
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
    if (timestamp_alerta > 0 && (time(NULL) - timestamp_alerta) < DURACION_ALERTA_SEGUNDOS) {
        attron(A_BOLD);
        mvprintw(panel_start_y + 5, panel_start_x, "   %s", mensaje_alerta);
        attroff(A_BOLD);
    } else {
        mensaje_alerta[0] = '\0';
        timestamp_alerta = 0;
        mvprintw(panel_start_y + 5, panel_start_x, " ================================================== ");
    }
    pthread_mutex_unlock(&mutex_nave);
    
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
        pthread_mutex_lock(&mutex_nave);
        if (nave_compartida->estado == ESTADO_VIVO) {
            nave_compartida->oxigeno -= 1;
            if (nave_compartida->oxigeno <= 0) {
                nave_compartida->oxigeno = 0;
                nave_compartida->estado = ESTADO_GAMEOVER_OXIGENO;
            }
        }
        pthread_mutex_unlock(&mutex_nave);
    }
    return NULL;
}

void* hilo_aviso_estacion() {
    msg_communication_station_warning message;
    while (juego_ejecutando) {
        ssize_t n = mq_receive(cola_aviso_estacion, (char *)&message, sizeof(message), 0);
        if (n == -1) {
            // Evaluamos si el error no es debido al cierre forzoso del juego
            if (juego_ejecutando) {
                perror("While receiving message from station warning queue");
            }
            break;
        }

        if (n == sizeof(message)) {
            pthread_mutex_lock(&mutex_nave);
            snprintf(mensaje_alerta, sizeof(mensaje_alerta), 
         "ALERTA! Estación en [%d,%d] baja de combustible (%d%%)", message.pos_x_station, message.pos_y_station, message.fuel_station);
            timestamp_alerta = time(NULL); 
            pthread_mutex_unlock(&mutex_nave);
        }
    }
    return NULL;
}

// --- ENVIAR COMANDO AL SERVIDOR ---

void enviar_comando(char cmd) {
    msg_communication_movement msg;
    msg.shipPid = getpid();

    int currentX = 0, currentY = 0;
    if (!obtener_posicion_shm(&currentX, &currentY)) {
        return; 
    }

    if (cmd == 'M') {
        int asteroidX = 0, asteroidY = 0;
        
        if (buscar_entidad_adyacente(currentX, currentY, ASTEROID, &asteroidX, &asteroidY)) {
            msg_communication_extraction ext_msg;
            ext_msg.shipPid = getpid();
            ext_msg.shipCurrentX = currentX;
            ext_msg.shipCurrentY = currentY;
            ext_msg.asteroidXPosition = asteroidX;
            ext_msg.asteroidYPosition = asteroidY;
            
            ext_msg.deuterioQuantity = 5;
            ext_msg.kernelioQuantity = 5;
            ext_msg.mutexioQuantity = 5;
            ext_msg.semaforitaQuantity = 5;

            if (mq_send(cola_extraccion, (const char*)&ext_msg, sizeof(ext_msg), 0) == -1) {
                perror("Failed to send extraction packet");
            }
        }
        return;
    }

    int targetX = currentX;
    int targetY = currentY;

    switch (cmd) {
        case 'U':
            targetY -= 1;
            break; // Up
        case 'D':
            targetY += 1;
            break; // Down
        case 'L':
            targetX -= 1;
            break; // Left
        case 'R':
            targetX += 1;
            break; // Right
    }

    if (targetX >= 0 && targetX < DEFAULT_MAP_WIDTH && targetY >= 0 && targetY < DEFAULT_MAP_HEIGHT) {
        msg.shipCurrentX = currentX;
        msg.shipCurrentY = currentY;
        msg.shipXMovement = targetX;
        msg.shipYMovement = targetY;
        
        if (mq_send(cola_movimiento, (const char*)&msg, sizeof(msg), 0) == -1) {
            perror("Failed to send movement packet");
        }
    }
}

int buscar_entidad_adyacente(int shipX, int shipY, EntityType objetivo, int *targetX, int *targetY) {
    // Offset vectors for: Up, Down, Left, Right
    int dx[] = { 0,  0, -1,  1};
    int dy[] = {-1,  1,  0,  0};

    for (int i = 0; i < 4; i++) {
        int checkX = shipX + dx[i];
        int checkY = shipY + dy[i];

        // Check map boundaries
        if (checkX >= 0 && checkX < DEFAULT_MAP_WIDTH && checkY >= 0 && checkY < DEFAULT_MAP_HEIGHT) {
            if (gameMap->map[checkY][checkX].typeStored == objetivo) {
                *targetX = checkX;
                *targetY = checkY;
                return 1; // Found it!
            }
        }
    }
    return 0; // No matching adjacent entity
}

int obtener_posicion_shm(int *actualX, int *actualY) {
    pid_t mi_pid = getpid();
    for (int y = 0; y < DEFAULT_MAP_HEIGHT; y++) {
        for (int x = 0; x < DEFAULT_MAP_WIDTH; x++) {
            if (gameMap->map[y][x].typeStored == SHIP && gameMap->map[y][x].ship.id == mi_pid) {
                *actualX = x;
                *actualY = y;
                return 1;
            }
        }
    }
    return 0;
}

// === NUEVO: LOGICA DE INTERACCIÓN DIRECTA CON LA ESTACIÓN ===
void interactuar_con_estacion() {
    int currentX = 0, currentY = 0;
    int estacionX = 0, estacionY = 0;

    if (!obtener_posicion_shm(&currentX, &currentY)) return;

    // Verificar si hay una estación en una casilla adyacente
    if (buscar_entidad_adyacente(currentX, currentY, STATION, &estacionX, &estacionY)) {
        
        pthread_mutex_lock(&mutex_nave);
        // Consolidamos la cantidad total de materiales a entregar (Deuterio y Mutexio)
        int recursosATrabajar = mi_nave.inventario.deuterio + mi_nave.inventario.mutexio;
        
        if (recursosATrabajar <= 0) {
            snprintf(mensaje_alerta, sizeof(mensaje_alerta), "ERROR: Sin recursos (D o M) para cambiar.");
            timestamp_alerta = time(NULL);
            pthread_mutex_unlock(&mutex_nave);
            return;
        }
        pthread_mutex_unlock(&mutex_nave);

        // Generar la ruta hacia la cola privada de la estación objetivo
        char q_estacion_name[60];
        snprintf(q_estacion_name, sizeof(q_estacion_name), "/mq_estacion_%d_%d", estacionX, estacionY);
        
        mqd_t q_estacion = mq_open(q_estacion_name, O_WRONLY);
        if (q_estacion == (mqd_t)-1) {
            pthread_mutex_lock(&mutex_nave);
            snprintf(mensaje_alerta, sizeof(mensaje_alerta), "Estación fuera de servicio o llena.");
            timestamp_alerta = time(NULL);
            pthread_mutex_unlock(&mutex_nave);
            return;
        }

        // Preparar y enviar la solicitud de intercambio
        msg_intercambio_solicitud req;
        req.shipPid = getpid();
        req.recursosEntregados = recursosATrabajar;

        if (mq_send(q_estacion, (const char*)&req, sizeof(req), 0) == -1) {
            perror("Error enviando solicitud de intercambio");
            mq_close(q_estacion);
            return;
        }
        mq_close(q_estacion);

        // Bloquear la ejecución local esperando de forma síncrona la respuesta de la estación
        msg_intercambio_respuesta res;
        ssize_t bytes_recibidos = mq_receive(cola_aviso_estacion, (char*)&res, sizeof(res), NULL);
        
        if (bytes_recibidos >= 0 && res.intercambioExitoso) {
            pthread_mutex_lock(&mutex_nave);
            // La estación reabasteció con éxito los tanques y vació el cargamento procesado
            mi_nave.inventario.deuterio = 0;
            mi_nave.inventario.mutexio = 0;
            mi_nave.combustible = res.nuevoCombustibleNave;
            mi_nave.oxigeno = res.nuevoOxigenoNave;
            
            snprintf(mensaje_alerta, sizeof(mensaje_alerta), "¡Trueque Exitoso! Oxígeno y Combustible al 100%%");
            timestamp_alerta = time(NULL);
            pthread_mutex_unlock(&mutex_nave);
        }
    } else {
        pthread_mutex_lock(&mutex_nave);
        snprintf(mensaje_alerta, sizeof(mensaje_alerta), "No hay ninguna estación adyacente.");
        timestamp_alerta = time(NULL);
        pthread_mutex_unlock(&mutex_nave);
    }
}
// ==================================================================

// --- PROGRAMA PRINCIPAL ---

int main() {
    int ch;
    pthread_t thread_visual, thread_oxigeno, thread_aviso_estacion;

    // Conexión obligatoria al entorno IPC antes de inicializar ncurses
    conectar_ipc();

    // Seteamos la nave local con su PID
    inicializar_nave(&mi_nave);

    msg_communication_initialization init_msg;
    init_msg.typeStored = SHIP;
    init_msg.ship = mi_nave; 
    if (mq_send(cola_inicializacion, (const char*)&init_msg, sizeof(init_msg), 0) == -1) {
        perror("Failed to send initialization message to server");
        exit(1);
    }

    sleep(1);

    inicializar_visuales();

    pthread_create(&thread_visual, NULL, hilo_renderizador, (void*)&mi_nave);
    pthread_create(&thread_oxigeno, NULL, hilo_soporte_vital, (void*)&mi_nave);
    pthread_create(&thread_aviso_estacion, NULL, hilo_aviso_estacion, NULL);

    // Bucle de Input: Lee teclas y las despacha por la Cola de Mensajes
    while ((ch = getch()) != TECLA_SALIR) {
        
        pthread_mutex_lock(&mutex_nave);
        int puede_moverse = (mi_nave.estado == ESTADO_VIVO && mi_nave.combustible > 0);
        
        // Variable testigo para identificar si el input corresponde a un intercambio
        int es_intercambio = 0; 

        if (puede_moverse) {
            switch (ch) {
                case KEY_UP:    caracter_nave = '^'; mi_nave.combustible -= 2; break;
                case KEY_DOWN:  caracter_nave = 'v'; mi_nave.combustible -= 2; break;
                case KEY_LEFT:  caracter_nave = '<'; mi_nave.combustible -= 2; break;
                case KEY_RIGHT: caracter_nave = '>'; mi_nave.combustible -= 2; break;
                case TECLA_MINAR:
                case 'M':                            mi_nave.combustible -= 5; break;
                
                // === NUEVO: DETECCIÓN DE ENTRADA PARA INTERCAMBIO ===
                case TECLA_ESTACION:
                case 'E':     es_intercambio = 1;                            break;
                // ========================================================
                
                default:        puede_moverse = 0;                             break;
            }
        }
        pthread_mutex_unlock(&mutex_nave);

        // === NUEVO: EJECUCIÓN FUERA DEL LOCK DEL MUTEX LOCAL ===
        if (es_intercambio) {
            interactuar_con_estacion();
            continue; // Saltar el envío de comandos convencionales de movimiento al servidor
        }
        // ============================================================

        if (puede_moverse) {
            switch (ch) {
                case KEY_UP:    enviar_comando('U'); break;
                case KEY_DOWN:  enviar_comando('D'); break;
                case KEY_LEFT:  enviar_comando('L'); break;
                case KEY_RIGHT: enviar_comando('R'); break;
                case TECLA_MINAR:
                case 'M':       enviar_comando('M'); break;
            }
        }
    }

    // Apagado y limpieza total de recursos del Sistema Operativo
    juego_ejecutando = 0;           
    pthread_join(thread_visual, NULL); 
    pthread_join(thread_oxigeno, NULL); 

    pthread_cancel(thread_aviso_estacion); 
    pthread_join(thread_aviso_estacion, NULL);

    finalizar_visuales();
    destruir_nave(&mi_nave);
    desconectar_ipc();

    return 0;
}