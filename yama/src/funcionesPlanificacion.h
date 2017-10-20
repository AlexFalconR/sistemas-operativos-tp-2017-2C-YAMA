#ifndef SRC_PLANI2_H_
#define SRC_PLANI2_H_

#include <utils.h>
#include <commons/collections/list.h>

/* 									Estructuras									*/

/* Estructura utilizada para leer informacion recibida del filesystem */

typedef struct{
			uint32_t nroBloqueArch;
			uint32_t idNodo0;
			uint32_t nroBloqueNodo0;
			uint32_t idNodo1;
			uint32_t nroBloqueNodo1;
			uint32_t bytesOcupados;
	}__attribute__((packed)) t_bloqueRecv;

/* Estructura utiliazada para mantener las planificaciones */

	typedef struct{
		uint32_t idPlanifiacion;
		t_list planificacion;
	}t_planificacion;

/* Estructuras utilizada para enviar informacion a Master */

/* Utilizada para enviar la etapa de transformacion */
	typedef struct{
		uint32_t idNodo;
		uint32_t nroBloqueNodo;
		uint32_t puerto;
		uint32_t bytesOcupados;
		char* ip;
		char* archivoTransformacion;
	}t_transformacionMaster;

/* Utilizada para enviar la etapa de reduccion local */
	typedef struct{
		uint32_t idNodo;
		uint32_t puerto;
		char* ip;
		char* archivoTransformacion;
		char* archivoRedLocal;
	}t_reduccionLocalMaster;

/* Utilizada para enviar la etapa de reduccion global*/
	typedef struct{
		uint32_t idNodo;
		uint32_t puerto;
		uint32_t encargado;
		char* ip;
		char* archvoRedLocal;
		char* archivoRedGlobal;
	}t_reduccionGlobalMaster;
/* Estructura utilizada para mantener informacion actualizada de los workers
 * disponibles, se habilitan por informacion del filesystem y se deshabilitan
 * por informacion de master, ademas de mantener el workload, Qt, Qrg y disponibilidad
 */
	typedef struct{
		uint32_t habilitado;
		uint32_t disponibilidad;
		uint32_t idNodo;
		uint32_t puerto;
		uint32_t workLoad;
		uint32_t cant_transformaciones;
		uint32_t cant_red_globales;
		char* ip;
	}t_worker_Disponibles;

	typedef struct{
		uint32_t job;
		uint32_t master;
		uint32_t nodo;
		uint32_t bloque;
		uint32_t etapa;
		uint32_t estado;
		char* archivoTemp;
	}t_tabla_estados;

//t_job. Es la estructura que se va a mandar al hilo que agarre el master y haga el job.
	typedef struct{
		uint32_t job;
		uint32_t idMaster;
		int socketMaster;
	}t_job;

/* Array de workers, se ira cargando a medida que se conecten y FS informe
 * Asumimos un maximo de 30 workers posibles
*/

extern t_worker_Disponibles workers[30];
extern uint32_t job;
extern int idMaster;

/* Listas */
t_list *listaBloquesRecibidos, *listaNodosInvolucrados;
t_list *listaPlanTransformaciones, *listaPlanRedLocal, *listaPlanRedGlobal;
t_list *listaMasters,*listaTablaEstados;

/*									Firma de funciones						*/

/* funcion pionera del hilo de preplanificacion */
void preplanificarJob(t_job*);


void crearListas();


/* Inicializa el array de workers suponiendo que todos estan deshabilitados inicialmente */
void inicializarWorkers(void);

/* Agregara a la lista de nodosInvolucrados y a la listaBloques */
void guardarEnBloqueRecibidos(t_bloqueRecv* );

/*Verifica la existencia de los nodos del bloque en la lista de nodos involucrados */
void nodosCargados(int,int);
int existeIdNodo(int );

/* Funcion a utilizar en el list_sort */
bool ordenarPorDisponibilidad(void* nodo0, void* nodo1);

/* Carga el vector a utilizar por el clock y clockAux; libera la memoria utilizada
 * por la lista de nodos involucrados */
void cargarVector(int* vectorNodos, t_list *listaNodos);

/* Asignar trabajo de un bloque a un nodo bajo W-clock */
void planificarTransformaciones(int,int*, t_bloqueRecv*, int*,int*);

/*Verificar que el nodo apuntado por clock contenga el bloque deseado */
int nodoContieneBloque(t_bloqueRecv bloqueRecibido,int* nodosInvolucrados,int* clock);

/* Cargar la planificacion del un bloque */
void actualizarPlanificacion(t_bloqueRecv,int* ,int*);

/* cargar en nro de bloque correpondiente al nodo elegido */
int cargarNroBloque(t_bloqueRecv, int*, int*);

/* actualizar disponibilidad, workload y Qt del nodo elegido */
void actualizarWorker(int*,int* );

/* actualizar la tabla de estados de yama */
void actualizarTablaEstados(t_bloqueRecv,int*,int*);


/* Incrementar la disponibilidad de todos los workers involucrados por la
 * disponibilidad base, cuando al intentar asignarlo la misma era 0 */
void incrementarDisponibilidadWorkers(int, int*);

/* Incrementa el clock y si es el ultimo, vuelve al primero */
void desplazarClock(int* ,int );

/* Genera el archivo temporal*/
char* generarNombreArchivoTemporal(int job, int nodo, int bloque);

/* planificacion de reducciones locales */
void planificacionReduccionLocal();

void cargarInfoReduccionLocal(t_transformacionMaster *, t_reduccionLocalMaster *, char *);

void seleccionarTransformacionLocales(t_list*);

bool existeRedLocal(t_reduccionLocalMaster*, t_list*);

void cargarReduccionLocalTablaEstado(char*, int);

/* planificacion de reduccion global */
void planificacionReduccionGlobal(int,int*);

/* Cargar informancion del registro de reduccion global */
void cargarInfoReduccionGlobal(int,int ,t_reduccionGlobalMaster*,t_list*);

void cargarReduccionGlobalTablaEstados(int ,t_reduccionGlobalMaster*);

/* Actualizar workload de los nodos involucrados utilzando la formula */
void actualizarWorkload(int ,int *);

/* seleccionar nodo con menor workload */
int seleccionarNodoMenorCarga(int*, int);

void destruir_listas(void);
#endif /* SRC_PLANI2_H_ */