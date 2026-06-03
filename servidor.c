#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <defaultconfig.h>
#include <map.h>

#include <stdio.h>
/* shm_* stuff, and mmap() */
#include <sys/mman.h>
#include <sys/types.h>
/* exit() etc */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
/* for random() stuff */
#include <stdlib.h>
#include <time.h>

int NUMBER_STATIONS = 0;
int NUMBER_ASTEROIDS = 0;
int MAP_HEIGHT = 0;
int MAP_WIDTH = 0;
int MAXIMUM_QUANTITY_DEUTERIO = 0;
int MAXIMUM_QUANTITY_MUTEXIO = 0;
int MAXIMUM_QUANTITY_SEMAFORITA = 0;
int MAXIMUM_QUANTITY_KERNELIO = 0;

int shmFileDescriptor;
Map gameMap;

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
            if (strcmp(key, "NUMBER_OF_SPACE_STATIONS") == 0) {
                NUMBER_STATIONS = aoi(value);
            } else if (strcmp(key, "NUMBER_ASTEROIDS") == 0) {
                NUMBER_ASTEROIDS = aoi(value);
            } else if (strcmp(key, "MAP_WIDTH") == 0) {
                MAP_WIDTH = aoi(value);
            } else if (strcmp(key, "MAP_HEIGHT") == 0) {
                MAP_HEIGHT = aoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_DEUTERIO") == 0) {
                MAXIMUM_QUANTITY_DEUTERIO = aoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_MUTEXIO") == 0) {
                MAXIMUM_QUANTITY_MUTEXIO = aoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_SEMAFORITA") == 0) {
                MAXIMUM_QUANTITY_SEMAFORITA = aoi(value);
            } else if (strcmp(key, "MAXIMUM_QUANTITY_KERNELIO") == 0) {
                MAXIMUM_QUANTITY_KERNELIO = aoi(value);
            }
        }
    }
    
    fclose(file);
    return 0;
}

int makeMap() {

    int shared_seg_size = (1 * sizeof(Map));   /* want shared segment capable of storing 1 message */
    struct msg_s *shared_msg;      /* the shared segment, and head of the messages list */    

    /* creating the shared memory object    --  shm_open()
		O_EXCL  - share memory object with the given name already exists, return an error
		S_IRWXU - read, write, execute/search by owner 
		S_IRWXG - read, write, execute/search by group 
	*/
    shmFileDescriptor = shm_open(SHARED_MEMORY_PATH, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shmFileDescriptor < 0) {
        perror("In shm_open()");
        exit(1);
    }
    fprintf(stderr, "Created shared memory object %s\n", SHARED_MEMORY_PATH);

    return 0;
}




int main() {
    exit(EXIT_SUCCESS);
}
