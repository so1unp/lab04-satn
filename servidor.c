#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char * NUMBER_OF_SPACE_STATIONS = 0;
char * NUMBER_ASTEROIDS = 0;
char * MAP_WIDTH = 0;
char * MAP_HIGHT = 0;
char * MAXIMUM_QUANTITY_PEPENIO = 0;
char * MAXIMUM_QUANTITY_ROBERTERIO = 0;

int configurationReading () {
    FILE *file = fopen("config.txt", "r");
    if (file == NULL) {
        printf("Error opening configuration file\n");
        return 1;
    }
    
    char line[100];
    char key[50];
    char value[50];
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%[^=] = %s", key, value) == 2) {
            if (strcmp(key, "NUMBER_OF_SPACE_STATIONS") == 0) {
                NUMBER_OF_SPACE_STATIONS = value;
            } else if (strcmp(key, "NUMBER_ASTEROIDS") == 0) {
                NUMBER_ASTEROIDS = value;
            } else if (strcmp(key, "MAP_WIDTH") == 0) {
                MAP_WIDTH = value;
            } else if (strcmp(key, "MAP_HIGHT") == 0) {
                MAP_HIGHT = value;
            } else if (strcmp(key, "MAXIMUM_QUANTITY_PEPENIO") == 0) {
                MAXIMUM_QUANTITY_PEPENIO = value;
            } else {
                MAXIMUM_QUANTITY_ROBERTERIO = value;
            } 
        }
    }
    
    fclose(file);
    return 0;
}

int main() {
    exit(EXIT_SUCCESS);
}