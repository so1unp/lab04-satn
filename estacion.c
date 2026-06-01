#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_HANGAR_CAPACITY 3
#define MAX_FUEL_CAPACITY 500

/**
 * La estacion tiene su posisionamiento en x e y,
 * su nombre, la capacidad maxima de naves, la cantidad de naves
 * que contiene hasta el momento
 *
 */
typedef struct
{
    char name[30];
    int pos_x;
    int pos_y;

    // Control de Hangar y Señales
    int hangarCapacity;
    int currentShips;
    pid_t shipPids[MAX_HANGAR_CAPACITY]; // Arreglo para guardar los PID de las naves
    
    // Combustible
    int fuelCapacity;
    int currentFuel;

    // Estado de la Estación
    bool fuelWarningSent;  // true si ya notificó a las naves
    bool isDestroyed;      // true si se quedó sin combustible y explotó

} Estacion;

void mostrarEstacion(Estacion aEstacion);

int main(int argc, char *argv[])
{
    // Agregar código aquí.

    Estacion estacion;
    strcpy(estacion.name, "Estacion 1");
    estacion.pos_x = 10;
    estacion.pos_y = 20;
    estacion.hangarCapacity = MAX_HANGAR_CAPACITY;
    estacion.currentShips = 0;
    estacion.fuelCapacity = MAX_FUEL_CAPACITY;
    estacion.currentFuel = 0;
    estacion.isDestroyed = false;

    mostrarEstacion(estacion);
    
    // Termina la ejecución del programa.
    exit(EXIT_SUCCESS);
}

void mostrarEstacion(Estacion aEstacion)
{
    printf("Nombre: %s\n", aEstacion.name);
    printf("Posicion en x: %d\n", aEstacion.pos_x);
    printf("Posicion en y: %d\n", aEstacion.pos_y);
    printf("Capacidad de naves: %d\n", aEstacion.hangarCapacity);
    printf("Cantidad de naves: %d\n", aEstacion.currentShips);
    printf("Capacidad de combustible: %d\n", aEstacion.fuelCapacity);
    printf("Cantidad de combustible: %d\n", aEstacion.currentFuel);
    if(!aEstacion.isDestroyed){
        printf("Estado de la estacion: HABILITADA\n");
    } else {
        printf("Estado de la estacion: INHABILITADA\n");
    }
}
