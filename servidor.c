#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <defaultconfig.h>
#include <map.h>
#include <communication.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <asteroid.h>

int NUMBER_STATIONS = 0;
int NUMBER_ASTEROIDS = 0;
int MAXIMUM_QUANTITY_DEUTERIO = 0;
int MAXIMUM_QUANTITY_MUTEXIO = 0;
int MAXIMUM_QUANTITY_SEMAFORITA = 0;
int MAXIMUM_QUANTITY_KERNELIO = 0;

#define QUEUE_PERMISSIONS 0666

Map *gameMap;
int sharedMemoryFd;
int shared_seg_size = (1 * sizeof(Map));

mqd_t shipMovementCommunicationQueue;
mqd_t shipExtractionCommunicationQueue;
mqd_t stationWarningCommunicationQueue;
mqd_t clientInitializationCommunicationQueue;

pthread_t t_receiveShipMovementMessage;
pthread_t t_receiveShipExtractionMessage;
pthread_t t_receiveStationWarningMessage;
pthread_t t_receiveClientInitializationMessage;

void *receiveShipMovementMessage();
void *receiveShipExtractionMessage();
void *receiveStationWarningMessage();
void *receiveClientInitializationMessage();
void *handleShipExtraction(void *arg);
void *handleShipMovement(void *arg);
void *handleStationWarning(void *arg);
void *handleClientInitialization(void *arg);

int main() {
    srand(time(NULL));
    int endSignal = 0;  
    initializeSettings();

    while (endSignal != 100) {
        scanf("%d", &endSignal);
        if (endSignal == 50) {
            printMapOnStandardOutput();
            endSignal = 0;
        }
    } 

    closeSettings();
    exit(0);
}

int configurationReading () {
    FILE *file = fopen("config.txt", "r");
    if (file == NULL) {
        printf("Error opening configuration file\n");
        NUMBER_STATIONS = DEFAULT_NUMBER_STATIONS;
        NUMBER_ASTEROIDS = DEFAULT_NUMBER_ASTEROIDS;
        MAXIMUM_QUANTITY_DEUTERIO = DEFAULT_MAXIMUM_QUANTITY_DEUTERIO;
        MAXIMUM_QUANTITY_MUTEXIO = DEFAULT_MAXIMUM_QUANTITY_MUTEXIO;
        MAXIMUM_QUANTITY_SEMAFORITA = DEFAULT_MAXIMUM_QUANTITY_SEMAFORITA;
        MAXIMUM_QUANTITY_KERNELIO = DEFAULT_MAXIMUM_QUANTITY_KERNELIO;
        return -1;
    }
    
    char line[100];
    char key[50];
    char value[50];
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%[^=] = %s", key, value) == 2) {
            if (strcmp(key, "NUMBER_STATIONS") == 0) {
                NUMBER_STATIONS = atoi(value);
            } else if (strcmp(key, "NUMBER_ASTEROIDS") == 0) {
                NUMBER_ASTEROIDS = atoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_DEUTERIO") == 0) {
                MAXIMUM_QUANTITY_DEUTERIO = atoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_MUTEXIO") == 0) {
                MAXIMUM_QUANTITY_MUTEXIO = atoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_SEMAFORITA") == 0) {
                MAXIMUM_QUANTITY_SEMAFORITA = atoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_KERNELIO") == 0) {
                MAXIMUM_QUANTITY_KERNELIO = atoi(value);
            }
        }
    }
    
    fclose(file);
    return 0;
}

int generateAsteroids () {
    for (int i = 0; i < NUMBER_ASTEROIDS; i++) {
        int position_y = (rand() % (DEFAULT_MAP_HEIGHT - 2)) + 1;
        int position_x = (rand() % (DEFAULT_MAP_WIDTH - 2)) + 1;
        while(gameMap->map[position_y][position_x].typeStored == ASTEROID) {
            position_y = (rand() % (DEFAULT_MAP_HEIGHT - 2)) + 1;
            position_x = (rand() % (DEFAULT_MAP_WIDTH - 2)) + 1;
        }
        gameMap->map[position_y][position_x].asteroid.deuterio = rand() % (MAXIMUM_QUANTITY_DEUTERIO + 1);
        gameMap->map[position_y][position_x].asteroid.kernelio = rand() % (MAXIMUM_QUANTITY_KERNELIO + 1);
        gameMap->map[position_y][position_x].asteroid.mutexio = rand() % (MAXIMUM_QUANTITY_MUTEXIO + 1);
        gameMap->map[position_y][position_x].asteroid.semaforita = rand() % (MAXIMUM_QUANTITY_SEMAFORITA + 1);
        gameMap->map[position_y][position_x].asteroid.isActive = true;
        gameMap->map[position_y][position_x].typeStored = ASTEROID;
        sem_init(&(gameMap->map[position_y][position_x].asteroid.mutex), 0, 1);
        sem_wait(&(gameMap->map[position_y][position_x].mutex));
    }
}

int makeMap() {
    
    sharedMemoryFd = shm_open(SHARED_MEMORY_PATH, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (sharedMemoryFd < 0) {
        perror("In shm_open()");
        exit(1);
    }
    ftruncate(sharedMemoryFd, shared_seg_size);
    
    gameMap = (Map *)mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);
    if (gameMap == NULL) {
        perror("In mmap()");
        exit(1);
    }
    
    for (int i = 0; i < DEFAULT_MAP_HEIGHT; i++) {
        for (int j = 0; j < DEFAULT_MAP_WIDTH; j++) {
            gameMap->map[i][j].typeStored = EMPTY;
            if (sem_init(&(gameMap->map[i][j].mutex), 0, 1) != 0) {
                perror("Failed to initialize process-shared semaphore");
                exit(1);
            }
        }
    }
    
    generateAsteroids();
    
    return 0;
}

int createQueues() {
    struct mq_attr attr;

    attr.mq_flags = O_NONBLOCK;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = (1*sizeof(msg_communication_movement));
    attr.mq_curmsgs = 0;
    shipMovementCommunicationQueue = mq_open(SERVER_MOVEMENT_COMMUNICATION_QUEUE_PATH, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS, &attr);
    if (shipMovementCommunicationQueue == (mqd_t)-1) {
        perror("Error creating movement queue");
        exit(1);
    }

    attr.mq_msgsize = (1*sizeof(msg_communication_extraction));
    shipExtractionCommunicationQueue = mq_open(SERVER_EXTRACTION_COMMUNICATION_QUEUE_PATH, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS, &attr);
    if (shipExtractionCommunicationQueue == (mqd_t)-1) {
        perror("Error creating movement queue");
        exit(1);
    }

    attr.mq_msgsize = (1*sizeof(msg_communication_station_warning));
    stationWarningCommunicationQueue = mq_open(SERVER_STATION_WARNING_COMMUNICATION_QUEUE_PATH, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS, &attr);
    if (stationWarningCommunicationQueue == (mqd_t)-1) {
        perror("Error creating warning queue");
        exit(1);
    }

    attr.mq_msgsize = (1*sizeof(msg_communication_initialization));
    clientInitializationCommunicationQueue = mq_open(SERVER_CLIENT_INITIALIZATION_QUEUE_PATH, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS, &attr);
    if (clientInitializationCommunicationQueue == (mqd_t)-1) {
        perror("Error creating warning queue");
        exit(1);
    }
}

int printMapOnStandardOutput() {
    int totalCounter = 0;
    int asteroidCounter = 0;
    int shipsCounter = 0;
    int stationCounter = 0;
    for (int i = 0; i < DEFAULT_MAP_HEIGHT; i++) {
        for (int j = 0; j < DEFAULT_MAP_WIDTH; j++) {
            printf("%d", gameMap->map[i][j].typeStored);
            totalCounter++;

            if(gameMap->map[i][j].typeStored == ASTEROID) {
                asteroidCounter++;
            }

            if(gameMap->map[i][j].typeStored == SHIP) {
                shipsCounter++;
            }

            if(gameMap->map[i][j].typeStored == STATION) {
                stationCounter++;
            }

        }
    }
    printf("\nTotal cells: %d\n", totalCounter);
    printf("\nTotal asteroids: %d\n", asteroidCounter);
    printf("\nTotal ships: %d\n", shipsCounter);
    printf("\nTotal stations: %d\n", stationCounter);
}

int initializeSettings() {
    configurationReading();
    makeMap();
    createQueues();

    if (pthread_create(&t_receiveShipMovementMessage,NULL,(void *)receiveShipMovementMessage,NULL) != 0) {
        perror("Failed to create thread for receiveShipMovementMessage");
    }
  
    if (pthread_create(&t_receiveClientInitializationMessage,NULL,(void *)receiveClientInitializationMessage,NULL) != 0) {
        perror("Failed to create thread for receiveShipMovementMessage");
    }
  
    if (pthread_create(&t_receiveShipExtractionMessage,NULL,(void *)receiveShipExtractionMessage,NULL) != 0) {
        perror("Failed to create thread for receiveShipExtractionMessage");
    }

    if (pthread_create(&t_receiveStationWarningMessage,NULL,(void *)receiveStationWarningMessage,NULL) != 0) {
        perror("Failed to create thread for receiveStationWarningMessage");
    }
}

int closeSettings() {
    if (shm_unlink(SHARED_MEMORY_PATH) != 0) {
        perror("In shm_unlink()");
        exit(1);
    }
    mq_unlink(SERVER_MOVEMENT_COMMUNICATION_QUEUE_PATH);
    mq_unlink(SERVER_EXTRACTION_COMMUNICATION_QUEUE_PATH);
    mq_unlink(SERVER_STATION_WARNING_COMMUNICATION_QUEUE_PATH);
    mq_unlink(SERVER_CLIENT_INITIALIZATION_QUEUE_PATH);

    pthread_cancel(t_receiveShipMovementMessage);
    pthread_cancel(t_receiveShipExtractionMessage);
    pthread_cancel(t_receiveStationWarningMessage);
    pthread_cancel(t_receiveClientInitializationMessage);

}

void *receiveShipMovementMessage() {
    msg_communication_movement message;
    while (1) {
        ssize_t n = mq_receive(shipMovementCommunicationQueue, (char *)&message, sizeof(message), 0);
        if (n == -1) {
            perror("While receiving message from ship movement queue");
            exit(1);
        }

        if (n == sizeof(message)) {
            printf("received a message from the movement queue\n");

            msg_communication_movement *shipMovement = malloc(sizeof(message));
            if (shipMovement == NULL) {
                perror("Failed to allocate memory for handleShipMovement thread");
                exit(1);
            }

            *shipMovement = message;
            pthread_t t_handleShipMovement;
            if (pthread_create(&t_handleShipMovement, NULL, (void *)handleShipMovement, shipMovement) != 0) {
                perror("Failed to create thread for handleShipMovement");
                free(shipMovement);
                continue;
            }

            pthread_detach(t_handleShipMovement);

        }

    }
}

void *handleShipMovement(void *arg) {
    msg_communication_movement *shipMovement = (msg_communication_movement *)arg;

    int shipOriginalXPosition = shipMovement->shipCurrentX;
    int shipOriginalYPosition = shipMovement->shipCurrentY;
    int shipNewXPosition = shipMovement->shipXMovement;
    int shipNewYPosition = shipMovement->shipYMovement;

    if (sem_trywait(&gameMap->map[shipNewYPosition][shipNewXPosition].mutex) == 0) {
        gameMap->map[shipNewYPosition][shipNewXPosition].typeStored = SHIP;
        gameMap->map[shipNewYPosition][shipNewXPosition].ship = gameMap->map[shipOriginalYPosition][shipOriginalXPosition].ship;

        gameMap->map[shipNewYPosition][shipNewXPosition].ship.pos_x = shipNewXPosition;
        gameMap->map[shipNewYPosition][shipNewXPosition].ship.pos_y = shipNewYPosition;

        gameMap->map[shipOriginalYPosition][shipOriginalXPosition].typeStored = EMPTY;
        sem_post(&gameMap->map[shipOriginalYPosition][shipOriginalXPosition].mutex);
    }   else {
        printf("REEEEEE");
    }

    free(shipMovement);

}

void *receiveShipExtractionMessage() {
    msg_communication_extraction message;

    while (1) {
        ssize_t n = mq_receive(shipExtractionCommunicationQueue, (char *)&message, sizeof(message), 0);
        if (n == -1) {
            perror("While receiving message from ship extraction queue");
            exit(1);
        }

        if (n == sizeof(message)) {
            printf("received a message from the extraction queue\n");

            msg_communication_extraction *shipExtraction = malloc(sizeof(message));
            if (shipExtraction == NULL) {
                perror("Failed to allocate memory for handleShipExtraction thread");
                exit(1);
            }

            *shipExtraction = message;
            pthread_t t_handleShipExtraction;
            if (pthread_create(&t_handleShipExtraction, NULL, (void *)handleShipExtraction, shipExtraction) != 0) {
                perror("Failed to create thread for handleShipExtraction");
                free(shipExtraction);
                continue;
            }

            pthread_detach(t_handleShipExtraction);

        }
 
    }
}

void *handleShipExtraction(void *arg) {
    msg_communication_extraction *shipExtraction = (msg_communication_extraction *)arg;
    printf("Hilo para extraer funciona\n");
    if (sem_wait(&gameMap->map[shipExtraction->asteroidYPosition][shipExtraction->asteroidXPosition].asteroid.mutex) == 0) {
        MapCell *asteroidCell = &gameMap->map[shipExtraction->asteroidYPosition][shipExtraction->asteroidXPosition];
        if (asteroidCell->typeStored == ASTEROID && asteroidCell->asteroid.isActive) {

            int amountDeuterio = 0;
            if (asteroidCell->asteroid.deuterio >= shipExtraction->deuterioQuantity) {
                amountDeuterio = shipExtraction->deuterioQuantity;
            }   else {
                amountDeuterio = asteroidCell->asteroid.deuterio;
            }
            asteroidCell->asteroid.deuterio -= amountDeuterio;
            printf("Deuterio extraido\n");

            int amountKernelio = 0;
            if (asteroidCell->asteroid.kernelio >= shipExtraction->kernelioQuantity) {
                amountKernelio = shipExtraction->kernelioQuantity;
            }   else {
                amountKernelio = asteroidCell->asteroid.kernelio;
            }
            asteroidCell->asteroid.kernelio -= amountKernelio;
printf("Kernelio extraido\n");

            int amountMutexio = 0;
            if (asteroidCell->asteroid.mutexio >= shipExtraction->mutexioQuantity) {
                amountMutexio = shipExtraction->mutexioQuantity;
            }   else {
                amountMutexio = asteroidCell->asteroid.mutexio;
            }
            asteroidCell->asteroid.mutexio -= amountMutexio;
printf("MUtexio extraido\n");

            int amountSemaforita = 0;
            if (asteroidCell->asteroid.semaforita >= shipExtraction->semaforitaQuantity) {
                amountSemaforita = shipExtraction->semaforitaQuantity;
            }   else {
                amountSemaforita = asteroidCell->asteroid.semaforita;
            }
            asteroidCell->asteroid.semaforita -= amountSemaforita;
printf("Semaforita extraido\n");

            if (asteroidCell->asteroid.deuterio == 0 && 
                asteroidCell->asteroid.kernelio == 0 && 
                asteroidCell->asteroid.mutexio == 0 && 
                asteroidCell->asteroid.semaforita == 0) {
                
                asteroidCell->asteroid.isActive = false;
                asteroidCell->typeStored = EMPTY;
                sem_post(&asteroidCell->mutex);
                printf("Asteroide sin recursos\n");
            }

            if (sem_wait(&gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].ship.mutex) == 0) {
                if (gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].typeStored == SHIP && gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].ship.id == shipExtraction->shipPid) {
                    
                    gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].ship.inventario.deuterio += amountDeuterio;
                    gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].ship.inventario.kernelio += amountKernelio;
                    gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].ship.inventario.mutexio += amountMutexio;
                    gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].ship.inventario.semaforita += amountSemaforita;
                }
                sem_post(&gameMap->map[shipExtraction->shipCurrentY][shipExtraction->shipCurrentX].ship.mutex);
                printf("Nave actualizada\n");
            }

        }

        sem_post(&gameMap->map[shipExtraction->asteroidYPosition][shipExtraction->asteroidXPosition].asteroid.mutex);
    }
    free(shipExtraction);

}

void *receiveStationWarningMessage() {
    msg_communication_station_warning message;
    while (1) {
        ssize_t n = mq_receive(stationWarningCommunicationQueue, (char *)&message, sizeof(message), 0);
        if (n == -1) {
            perror("While receiving message from station warning queue");
            exit(1);
        }

        if (n == sizeof(message)) {
            printf("received a message from the station warning queue\n");

            msg_communication_station_warning *stationWarning = malloc(sizeof(message));
            if (stationWarning == NULL) {
                perror("Failed to allocate memory for handleStationWarning thread");
                exit(1);
            }

            *stationWarning = message;
            pthread_t t_handleStationWarning;
            if (pthread_create(&t_handleStationWarning, NULL, (void *)handleStationWarning, stationWarning) != 0) {
                perror("Failed to create thread for handleStationWarning");
                free(stationWarning);
                continue;
            }

            pthread_detach(t_handleStationWarning);
        }

    }
}

void *handleStationWarning(void *arg) {
    msg_communication_station_warning *stationWarning = (msg_communication_station_warning *)arg;
    char shipQueuePath[128];

    for (int i = 0; i < DEFAULT_MAP_HEIGHT; i++) {
        for (int j = 0; j < DEFAULT_MAP_WIDTH; j++) {
            
            if (gameMap->map[i][j].typeStored == SHIP && gameMap->map[i][j].ship.estado == ESTADO_VIVO) {
                snprintf(shipQueuePath, sizeof(shipQueuePath), "%s%d", SHIP_BASE_PATH, gameMap->map[i][j].ship.id);

                mqd_t shipQueueFileDescriptor= mq_open(shipQueuePath, O_WRONLY);
                
                if (shipQueueFileDescriptor != (mqd_t)-1) {
                    if (mq_send(shipQueueFileDescriptor, (const char *)stationWarning, sizeof(msg_communication_station_warning), 0) == -1) {
                        perror("Failed to send an alert message to ship queue");
                    }
                    mq_close(shipQueueFileDescriptor);
                }
            }

        }
    }

    free(stationWarning);
}

void *receiveClientInitializationMessage() {
    msg_communication_initialization message;
    while (1) {
        ssize_t n = mq_receive(clientInitializationCommunicationQueue, (char *)&message, sizeof(message), 0);
        if (n == -1) {
            perror("While receiving message from client initialization queue");
            exit(1);
        }

        if (n == sizeof(message)) {
            printf("received a message from the client initialization queue\n");

            msg_communication_initialization *clientInitialization = malloc(sizeof(message));
            if (clientInitialization == NULL) {
                perror("Failed to allocate memory for handleClientInitialization thread");
                exit(1);
            }

            *clientInitialization = message;
            pthread_t t_handleClientInitialization;
            if (pthread_create(&t_handleClientInitialization, NULL, (void *)handleClientInitialization, clientInitialization) != 0) {
                perror("Failed to create thread for handleClientInitialization");
                free(clientInitialization);
                continue;
            }

            pthread_detach(t_handleClientInitialization);
        }

    }
}

void *handleClientInitialization(void *arg) {
    msg_communication_initialization *clientInitialization = (msg_communication_initialization *)arg;

    int chosenX;
    int chosenY;
    bool wasPlaced = false;

    while (!wasPlaced) {
        chosenY = rand() % DEFAULT_MAP_HEIGHT;
        chosenX = rand() % DEFAULT_MAP_WIDTH;

        if (sem_trywait(&gameMap->map[chosenY][chosenX].mutex) == 0) {
            if (clientInitialization->typeStored == SHIP) {
                gameMap->map[chosenY][chosenX].typeStored = SHIP;
                gameMap->map[chosenY][chosenX].ship = clientInitialization->ship;
                sem_init(&(gameMap->map[chosenY][chosenX].ship.mutex), 1, 1);
            } else if (clientInitialization->typeStored == STATION) {
                gameMap->map[chosenY][chosenX].typeStored = STATION;
//                gameMap->map[chosenX][chosenY].station = clientInitialization->station;
            }

            wasPlaced = true;
        }
    }

    free(clientInitialization);
}
