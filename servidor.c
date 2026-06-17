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
int MAP_HEIGHT = DEFAULT_MAP_HEIGHT;
int MAP_WIDTH = DEFAULT_MAP_WIDTH;
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
        int position_x = (rand() % (MAP_WIDTH-2))+1;
        int position_y = (rand() % (MAP_HEIGHT-2))+1;
        while(gameMap->map[position_x][position_y].typeStored == ASTEROID) {
            position_x = (rand() % (MAP_WIDTH-2))+1;
            position_y = (rand() % (MAP_HEIGHT-2))+1;
        }
        
        gameMap->map[position_x][position_y].asteroid.deuterio = rand() % (MAXIMUM_QUANTITY_DEUTERIO + 1);
        gameMap->map[position_x][position_y].asteroid.kernelio = rand() % (MAXIMUM_QUANTITY_KERNELIO + 1);
        gameMap->map[position_x][position_y].asteroid.mutexio = rand() % (MAXIMUM_QUANTITY_MUTEXIO + 1);
        gameMap->map[position_x][position_y].asteroid.semaforita = rand() % (MAXIMUM_QUANTITY_SEMAFORITA + 1);
        gameMap->map[position_x][position_y].typeStored = ASTEROID;
        
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
    
    for (int i = 0; i < DEFAULT_MAP_WIDTH; i++) {
        for (int j = 0; j < DEFAULT_MAP_HEIGHT; j++) {
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
}

int printMap() {
    int counter = 0;
    int counterAsteroid = 0;
    for (int i = 0; i < MAP_WIDTH; i++) {
        for (int j = 0; j < MAP_HEIGHT; j++) {
            printf("%d", gameMap->map[i][j].typeStored);
            counter++;

            if(gameMap->map[i][j].typeStored == ASTEROID)
                counterAsteroid++;
        }
    }
    printf("\n%d", counter);
    printf("\n%d", counterAsteroid);
}

int initializeSettings() {
    configurationReading();
    makeMap();
    createQueues();
}

int closeSettings() {
    if (shm_unlink(SHARED_MEMORY_PATH) != 0) {
        perror("In shm_unlink()");
        exit(1);
    }
    mq_unlink(SERVER_MOVEMENT_COMMUNICATION_QUEUE_PATH);
    mq_unlink(SERVER_EXTRACTION_COMMUNICATION_QUEUE_PATH);
    mq_unlink(SERVER_STATION_WARNING_COMMUNICATION_QUEUE_PATH);

}

int main() {
    int endSignal = 0;  
    initializeSettings();
    while (endSignal != 100) {
        scanf("%d", &endSignal);
    } 

    closeSettings();
/* 

   if ((cola = mq_open (argv[1],  O_RDWR | O_NONBLOCK )) == -1) 
   if ((cola = mq_open (argv[1],  O_RDWR )) == -1) 
*/
    exit(0);

}
