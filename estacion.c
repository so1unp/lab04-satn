/**
 * @file estacion.c
 * @brief Proceso que representa y gestiona el ciclo de vida de una estación espacial.
 *
 * Flujo general:
 *   main() → inicializar_estacion() → conectar_ipc() → [hilos]
 *     - thread_station:         consume_fuel() → send_warning_message_servidor()
 *     - thread_station_trading: handle_trading() → registrar_bitacora()
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <errno.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

#include "estacion.h"
#include "header/defaultconfig.h"
#include <communication.h>
#include <map.h>

/* ═══════════════════════════════════════════════════════
 * VARIABLES GLOBALES
 * ═══════════════════════════════════════════════════════ */

station  my_station;
Map     *gameMap        = NULL;
int      sharedMemoryFd = -1;

/* --- Hilos --- */
pthread_t thread_station;
pthread_t thread_station_trading;

/* --- Colas de mensajes --- */
mqd_t cola_inicializacion        = (mqd_t)-1;
mqd_t cola_aviso_poco_combustible = (mqd_t)-1;
mqd_t cola_trading               = (mqd_t)-1;

/* ═══════════════════════════════════════════════════════
 * PROTOTIPOS
 * ═══════════════════════════════════════════════════════ */

void  inicializar_estacion(station *a_station, int pos_x, int pos_y);
void  conectar_ipc(void);
void  desconectar_ipc(void);
void  send_warning_message_servidor(station *a_station);
void  consume_fuel(station *a_station);
void  registrar_bitacora(int est_x, int est_y, pid_t nave_pid,
                                int deut, int mut, int sema, int kern);
void *run_station(void *arg);
void *handle_trading(void *arg);

/* ═══════════════════════════════════════════════════════
 * INICIALIZACIÓN Y CONEXIÓN
 * ═══════════════════════════════════════════════════════ */

/**
 * @brief Inicializa los valores por defecto de la estación.
 */
void inicializar_estacion(station *a_station, int pos_x, int pos_y) {
    if (a_station == NULL) return;

    a_station->pos_x         = pos_x;
    a_station->pos_y         = pos_y;
    a_station->fuel          = 100;
    a_station->fuelWarningSent = false;
    a_station->isDestroyed   = false;
    a_station->ship_message  = false;
}

/**
 * @brief Conecta la estación a la memoria compartida y las colas POSIX del servidor.
 */
void conectar_ipc(void) {
    sharedMemoryFd = shm_open(SHARED_MEMORY_PATH, O_RDWR, 0666);
    if (sharedMemoryFd < 0) {
        perror("[Estación] shm_open — ¿el servidor está corriendo?");
        exit(1);
    }

    gameMap = (Map *)mmap(NULL, sizeof(Map), PROT_READ | PROT_WRITE,
                          MAP_SHARED, sharedMemoryFd, 0);
    if (gameMap == MAP_FAILED) {
        perror("[Estación] mmap");
        exit(1);
    }

    cola_aviso_poco_combustible = mq_open(
        SERVER_STATION_WARNING_COMMUNICATION_QUEUE_PATH, O_WRONLY | O_NONBLOCK);
    if (cola_aviso_poco_combustible == (mqd_t)-1) {
        perror("[Estación] mq_open cola de advertencia");
        exit(1);
    }

    cola_inicializacion = mq_open(SERVER_CLIENT_INITIALIZATION_QUEUE_PATH, O_WRONLY);
    if (cola_inicializacion == (mqd_t)-1) {
        perror("[Estación] mq_open cola de inicialización");
        exit(1);
    }
}

/**
 * @brief Libera todos los recursos IPC abiertos por la estación.
 */
void desconectar_ipc(void) {
    if (cola_aviso_poco_combustible != (mqd_t)-1) {
        mq_close(cola_aviso_poco_combustible);
        cola_aviso_poco_combustible = (mqd_t)-1;
    }
    if (cola_inicializacion != (mqd_t)-1) {
        mq_close(cola_inicializacion);
        cola_inicializacion = (mqd_t)-1;
    }
    if (cola_trading != (mqd_t)-1) {
        mq_close(cola_trading);
        cola_trading = (mqd_t)-1;
    }
    if (gameMap != NULL && gameMap != MAP_FAILED) {
        munmap(gameMap, sizeof(Map));
        gameMap = NULL;
    }
    if (sharedMemoryFd >= 0) {
        close(sharedMemoryFd);
        sharedMemoryFd = -1;
    }
}

/* ═══════════════════════════════════════════════════════
 * LÓGICA DE LA ESTACIÓN
 * ═══════════════════════════════════════════════════════ */

/**
 * @brief Envía una alerta de bajo combustible al servidor de forma no bloqueante.
 */
void send_warning_message_servidor(station *a_station) {
    if (a_station == NULL) return;

    msg_communication_station_warning msg;
    msg.station_pid    = getpid();
    msg.pos_x_station  = a_station->pos_x;
    msg.pos_y_station  = a_station->pos_y;
    msg.fuel_left      = a_station->fuel;

    printf("[Estación (%d,%d)] Enviando alerta al servidor — fuel: %d%%\n",
           msg.pos_x_station, msg.pos_y_station, msg.fuel_left);

    if (mq_send(cola_aviso_poco_combustible,
                (const char *)&msg,
                sizeof(msg_communication_station_warning), 0) == -1) {
        if (errno == EAGAIN)
            printf("[Estación] Cola del servidor llena, alerta descartada.\n");
        else
            perror("[Estación] mq_send alerta");
    }
}

/**
 * @brief Bucle principal de consumo de combustible.
 *        Envía alertas al servidor cuando el nivel baja de 40%.
 *        Termina cuando el combustible llega a 0.
 */
void consume_fuel(station *a_station) {
    if (a_station == NULL) {
        fprintf(stderr, "[Estación] consume_fuel: puntero nulo\n");
        exit(1);
    }

    while (a_station->fuel > 0) {
        unsigned int espera = TIME_OF_CONSUME_FUEL + (unsigned int)(rand() % 3);
        sleep(espera);

        a_station->fuel -= 10;
        if (a_station->fuel < 0)
            a_station->fuel = 0;

        if (a_station->fuel < 40)
            send_warning_message_servidor(a_station);

        if (a_station->ship_message) {
            printf("[Estación (%d,%d)] Trade iniciado por una nave.\n",
                   a_station->pos_x, a_station->pos_y);
            fflush(stdout);
            a_station->ship_message = false;
        }
    }
}

/**
 * @brief Registra una transacción de forma atómica en bitacora.txt.
 */
void registrar_bitacora(int est_x, int est_y, pid_t nave_pid,
                               int deut, int mut, int sema, int kern) {
    FILE *arch = fopen("bitacora.txt", "a");
    if (arch == NULL) {
        perror("[Estación] fopen bitacora.txt");
        return;
    }

    fprintf(arch,
            "[TRANSACTION] Estacion (%d,%d) -> Nave PID %d | "
            "Minerales Recibidos: D:%d, M:%d, S:%d, K:%d | Status: OK\n",
            est_x, est_y, nave_pid, deut, mut, sema, kern);

    fflush(arch);
    fclose(arch);
}

/* ═══════════════════════════════════════════════════════
 * HILOS
 * ═══════════════════════════════════════════════════════ */

/**
 * @brief Hilo principal de la estación.
 *        Sincroniza el estado en SHM, inicializa el semáforo del hangar
 *        y ejecuta el bucle de consumo de combustible.
 */
void *run_station(void *arg) {
    station *local_station = (station *)arg;

    station *shm = &(gameMap->map[local_station->pos_y][local_station->pos_x].a_station);

    shm->fuel         = local_station->fuel;
    shm->pos_x        = local_station->pos_x;
    shm->pos_y        = local_station->pos_y;
    shm->ship_message = false;
    shm->isDestroyed  = false;

    /* Inicializar el semáforo del hangar en SHM (pshared=1, capacidad=3) */
    sem_init(&(shm->hangar_sem), 1, 3);

    printf("[Estación (%d,%d)] Hilo iniciado. Consumo en curso...\n",
           shm->pos_x, shm->pos_y);

    consume_fuel(shm);

    printf("[Estación (%d,%d)] Combustible agotado.\n", shm->pos_x, shm->pos_y);
    send_warning_message_servidor(shm);

    shm->isDestroyed = true;

    return NULL;
}

/**
 * @brief Hilo de trading.
 *        Espera mensajes de naves en la cola POSIX de la estación,
 *        actualiza el combustible y registra cada transacción en la bitácora.
 */
void *handle_trading(void *arg) {
    station *local_station  = (station *)arg;
    station *shm = &(gameMap->map[local_station->pos_y][local_station->pos_x].a_station);

    msg_trade_request req;
    unsigned int      prio;

    while (!shm->isDestroyed) {
        ssize_t n = mq_receive(cola_trading, (char *)&req, sizeof(req), &prio);

        if (n == -1) {
            if (errno == EINTR) continue;
            perror("[Estación] handle_trading: mq_receive");
            break;
        }

        if (req.type == SELL_RESOURCES) {

            printf("[Estación (%d,%d)] Trade recibido de nave PID %d\n",
                   shm->pos_x, shm->pos_y, req.ship_pid);
    
            shm->fuel += req.deuterio_to_sell * 5;
            if (shm->fuel > 100) shm->fuel = 100;
    
            registrar_bitacora(shm->pos_x, shm->pos_y, req.ship_pid,
                               req.deuterio_to_sell, req.mutexio_to_sell,
                               req.semaforita_to_sell, req.kernelio_to_sell);
        }

    }

    return NULL;
}

/* ═══════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    srand((unsigned int)(time(NULL) ^ getpid()));

    if (argc < CANT_ARGUMENTS) {
        fprintf(stderr, "Uso: estacion <pos_x> <pos_y>\n");
        return 1;
    }

    int pos_x = atoi(argv[1]);
    int pos_y = atoi(argv[2]);

    if (pos_x < 0 || pos_y < 0) {
        fprintf(stderr, "[Estación] Error: posiciones no pueden ser negativas.\n");
        return 1;
    }

    /* 1. Inicialización local */
    inicializar_estacion(&my_station, pos_x, pos_y);

    /* 2. Cola de trading propia (antes de conectar_ipc para que exista al recibir naves) */
    char q_name[64];
    snprintf(q_name, sizeof(q_name), "/mq_estacion_%d_%d", pos_x, pos_y);

    struct mq_attr attr = {
        .mq_flags   = 0,
        .mq_maxmsg  = 5,
        .mq_msgsize = sizeof(msg_trade_request),
        .mq_curmsgs = 0
    };
    cola_trading = mq_open(q_name, O_CREAT | O_RDONLY, 0666, &attr);
    if (cola_trading == (mqd_t)-1) {
        perror("[Estación] mq_open cola_trading");
        return 1;
    }

    /* 3. Conexión al servidor */
    conectar_ipc();

    /* 4. Registro en el servidor */
    msg_communication_initialization init_msg;
    init_msg.typeStored = STATION;
    init_msg.a_station  = my_station;
    if (mq_send(cola_inicializacion, (const char *)&init_msg, sizeof(init_msg), 0) == -1) {
        perror("[Estación] mq_send inicialización");
        desconectar_ipc();
        return 1;
    }

    printf("[Estación (%d,%d)] Registrada en el servidor.\n", pos_x, pos_y);

    /* 5. Lanzar hilos */
    if (pthread_create(&thread_station, NULL, run_station, &my_station) != 0) {
        perror("[Estación] pthread_create thread_station");
        desconectar_ipc();
        return 1;
    }
    if (pthread_create(&thread_station_trading, NULL, handle_trading, &my_station) != 0) {
        perror("[Estación] pthread_create thread_station_trading");
        desconectar_ipc();
        return 1;
    }

    /* 6. Esperar a que la estación agote su combustible */
    pthread_join(thread_station, NULL);

    /* 7. Terminar hilo de trading */
    pthread_cancel(thread_station_trading);
    pthread_join(thread_station_trading, NULL);

    /* 8. Liberar recursos */
    desconectar_ipc();

    printf("[Estación (%d,%d)] Proceso finalizado.\n", pos_x, pos_y);
    return 0;
}