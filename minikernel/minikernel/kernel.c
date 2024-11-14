/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */
#include <string.h> // Para operaciones con strings

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	//printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	//printk("-> TRATANDO INT. DE RELOJ\n");
	
	//TRATAR PROCESOS DORMIDOS (SLEEP)
	BCPptr aux = lista_dormidos.primero;
	while(aux != NULL)
	{
		aux->segs_restantes -= 1;
		BCPptr aux2 = aux->siguiente;
		if(aux->segs_restantes == 0){
			int nivel = fijar_nivel_int(NIVEL_3);
			eliminar_elem(&(lista_dormidos), aux);
			insertar_ultimo(&(lista_listos), aux);
			fijar_nivel_int(nivel);
		}
		aux = aux2;
	}

	//COMPROBAR SI EL PROC. HA ACABADO SU ROJADA (ROUND ROBIN)
	p_proc_actual -> TICKS_por_rodaja--;
	if(p_proc_actual-> TICKS_por_rodaja <= 0)
	{
		p_proc_a_expulsar = p_proc_actual;
		activar_int_SW();
	}
	

    //return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	printk("-> TRATANDO INT. SW\n");
	if (p_proc_a_expulsar == p_proc_actual)
	{
		BCP* proceso_A = p_proc_actual;
		BCP* proceso_B;
		int nivel = fijar_nivel_int(1);

		eliminar_elem(&lista_listos, proceso_A);
		insertar_ultimo(&lista_listos, proceso_A);

		p_proc_actual=planificador();
		proceso_B = p_proc_actual;

		proceso_B->TICKS_por_rodaja = TICKS_POR_RODAJA;

		cambio_contexto(&(proceso_A->contexto_regs), &(proceso_B->contexto_regs));

		fijar_nivel_int(nivel);
	}
	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;
		p_proc->segs_restantes = 0;
		p_proc->TICKS_por_rodaja = TICKS_POR_RODAJA;

		// Para los mutex
		for(int i = 0; i < NUM_MUT_PROC; i++)
		{
			p_proc->descriptores[i] = -1;
		}
		p_proc->descriptores_abiertos = 0;

		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	printk("-> FIN PROCESO %d\n", p_proc_actual->id);

	// Buscar mutex que hay que cerrar al terminar un proceso
	for(int j = 0; j < NUM_MUT_PROC; j++)
	{
		// Comprobar que posiciones del array de descriptores tienen un mutex asignado
		if(p_proc_actual->descriptores[j] != -1)
		{
			escribir_registro(1, j);
			sis_cerrarMutex();
		}
	}
	liberar_proceso();

        return 0; /* no deber�a llegar aqui */
}

int sis_obtener_id(){

	return p_proc_actual->id;
}

int sis_dormir(){

	//se lee el parametro de la llamada (segundos)
	unsigned int segs = (unsigned int)leer_registro(1);

	//fijar nivel interrupcion
	int nivel = fijar_nivel_int(NIVEL_3);
	//asginar tiempo dormido y pasar a lista dormidos
	BCP *actual = p_proc_actual;
	actual->segs_restantes = segs*TICK;
	actual->estado = BLOQUEADO;
	eliminar_elem(&lista_listos, actual);
	insertar_ultimo(&lista_dormidos, actual);

	//comenzar nuevo proceso
	p_proc_actual = planificador();
	cambio_contexto(&(actual->contexto_regs), &(p_proc_actual->contexto_regs));

	//restaurar nivel interrupcion
	fijar_nivel_int(nivel);
	return 0;
}


// MUTEX

int sis_crearMutex(){

	char* nombre = (char*)leer_registro(1);
	int tipo = (int) leer_registro(2);

	int nivel = fijar_nivel_int(NIVEL_1);






	//COMPROBAR QUE EL NOMBRE ES VALIDO
	if(strlen(nombre) > MAX_NOM_MUT)
	{
		printf("Error: el nombre supera la longitud maxima");
		fijar_nivel_int(nivel);
		return -1;
	}

	//COMPROBAR QUE EL NOMBRE ESTA LIBRE
	for(int i = 0; i < NUM_MUT; i++)
	{
		if((strcmp(nombre, sis_lista_mutex[i].nombre)) == 0)
		{
			printf("Error: el nombre ya esta en uso");
			fijar_nivel_int(nivel);
			return -2;
		}
	}

	//COMPROBAR QUE HAY HUECO EN LA LISTA DE DESRIPTORES
	int descriptor_libre_encontrado = 0;
	int posicion_descriptor_libre = -1;
	int i  = 0;
	if(p_proc_actual->descriptores_abiertos < NUM_MUT_PROC)
	{
		
		while(descriptor_libre_encontrado == 0 && i < NUM_MUT_PROC)
		{
			if (p_proc_actual->descriptores[i] == -1)
			{
				descriptor_libre_encontrado = 1;
				posicion_descriptor_libre = i;
			}
			else{
				i++;
			}
		}
	}else
	{
		printf("Error: no hay hueco en la lista de descriptores");
		fijar_nivel_int(nivel);
		return -3;
	}

		//COMPROBAR QUE HAY MUTEX LIBRES
	int mutex_libre_encontrado = 0;
	int posicion_mutex_libre = -1;
	int i = 0;
	while(mutex_libre_encontrado == 0 && i < NUM_MUT)
	{
		if (sis_lista_mutex[i].estado == LIBRE)
		{
			mutex_libre_encontrado = 1;
			posicion_mutex_libre = i;
		}
		else{
			i++;
		}
	}
	if (mutex_libre_encontrado != 1)
	{
		//SI NO HAY MUTEX LIBRE, BLOQUEA EL PROCESO
		BCP* proc_a_bloquear = p_proc_actual;
		proc_a_bloquear->estado = BLOQUEADO;
		eliminar_primero(&lista_listos);
		insertar_ultimo(&lista_bloqueados_mutex, proc_a_bloquear);
		p_proc_actual = planificador();
		cambio_contexto(&(p_proc_a_expulsar->contexto_regs), &(p_proc_actual->contexto_regs));
		printf("Error: no hay mutex libres");
		fijar_nivel_int(nivel);
		return -4;
	}

	Mutex * mutex_a_crear = &(sis_lista_mutex[posicion_mutex_libre]);
	strcpy((mutex_a_crear->nombre),nombre);
	mutex_a_crear->tipo = tipo;
	mutex_a_crear->num_bloqueos = 0;
	mutex_a_crear->estado = OCUPADO;
	mutex_a_crear->num_procesos_bloqueados  = 0;
	mutex_a_crear->id_proc = p_proc_actual->id;

	//ABRIMOS EL MUTEX QUE ACABAMOS DE CREAR
	escribir_registro(1, (long) nombre);
	sis_abrirMutex();

	fijar_nivel_int(nivel);

	return posicion_descriptor_libre;
}

int sis_abrirMutex(){
	char * nombre  = leer_registro(1);

	//COMPROBAR SI EL NOMBRE EXISTE
	int i = 0;
	int mutex_encontrado = 0;
	while(i < NUM_MUT && mutex_encontrado)
	{
		if (strcmp(nombre, sis_lista_mutex[i].nombre) == 0)
		{
			mutex_encontrado = 1;
		}
		else{
			i++;
		}
	}
	if(mutex_encontrado = 0)
	{
		print("Error: nombre no válido");
		return -1;
	}

	
}

int sis_lockMutex(){
	return 0;
}

int sis_unlockMutex(){
	return 0;
}

int sis_cerrarMutex(){
	return 0;
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
