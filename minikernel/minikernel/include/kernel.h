/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
        int id;				/* ident. del proceso */
        int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
        contexto_t contexto_regs;	/* copia de regs. de UCP */
        void * pila;			/* dir. inicial de la pila */
		int segs_restantes;  /* segundos que le quedan al proceso para despertarse*/
		BCPptr siguiente;		/* puntero a otro BCP */
		void *info_mem;			/* descriptor del mapa de memoria */
		int TICKS_por_rodaja; /* Tick que tiene cada rodaja*/

		int descriptores[NUM_MUT_PROC];
		int descriptores_abiertos;
} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;

typedef struct Mutex_t{
	int tipo;
	char nombre[MAX_NOM_MUT];
	int estado;
	int num_bloqueos;
	int num_procesos_esperando;
	lista_BCPs lista_espera;
	BCP * proc_mut;
	//int proc_abiertos;
} Mutex;

/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */
BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/*
 * Variable global que representa la cola de procesos dormidos
 */
lista_BCPs lista_dormidos= {NULL, NULL};

/*
 * Variable global que representa procesos que seran expulsados por round robin
 */
BCP * p_proc_a_expulsar=NULL;

/*
 * Variable global que representa los mutex
 */
Mutex sis_lista_mutex[NUM_MUT];

/*
 * Variable global que representa la lista de procesos bloqueados
 */
lista_BCPs lista_bloqueados_mutex= {NULL, NULL};

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int sis_obtener_id();
int sis_dormir();
int sis_crearMutex();
int sis_abrirMutex();
int sis_lockMutex();
int sis_unlockMutex();
int sis_cerrarMutex();
/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{sis_obtener_id},
					{sis_dormir},
					{sis_crearMutex},
					{sis_abrirMutex},
					{sis_lockMutex},
					{sis_unlockMutex},
					{sis_cerrarMutex}};

#endif /* _KERNEL_H */

