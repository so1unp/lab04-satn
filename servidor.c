#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <defaultconfig.h>
#include <map.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int NUMBER_STATIONS = 0;
int NUMBER_ASTEROIDS = 0;
int MAP_HEIGHT = 0;
int MAP_WIDTH = 0;
int MAXIMUM_QUANTITY_DEUTERIO = 0;
int MAXIMUM_QUANTITY_MUTEXIO = 0;
int MAXIMUM_QUANTITY_SEMAFORITA = 0;
int MAXIMUM_QUANTITY_KERNELIO = 0;

Map *gameMap;
int sharedMemoryFd;
int shared_seg_size = (1 * sizeof(Map));

int configurationReading () {
    FILE *file = fopen("config.txt", "r");
    if (file == NULL) {
        printf("Error opening configuration file\n");
        NUMBER_STATIONS = DEFAULT_NUMBER_STATIONS;
        NUMBER_ASTEROIDS = DEFAULT_NUMBER_ASTEROIDS;
        MAP_HEIGHT = DEFAULT_MAP_HEIGHT;
        MAP_WIDTH = DEFAULT_MAP_WIDTH;
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
            } else if (strcmp(key, "MAP_WIDTH") == 0) {
                MAP_WIDTH = atoi(value);
            } else if (strcmp(key, "MAP_HEIGHT") == 0) {
                MAP_HEIGHT = atoi(value);
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

    return 0;
}

int printMap() {
    int counter = 0;
    for (int i = 0; i < MAP_WIDTH; i++) {
        for (int j = 0; j < MAP_HEIGHT; j++) {
            printf("%d", gameMap->map[i][j].typeStored);
            counter++;
        }
    }
    printf("\n%d", counter);
}

int main() {

    configurationReading();
    makeMap();
    printMap();
    return 0;
}
