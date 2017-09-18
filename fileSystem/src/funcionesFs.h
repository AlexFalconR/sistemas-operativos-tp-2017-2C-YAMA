/*
 * funcionesFs.h
 *
 *  Created on: 17/9/2017
 *      Author: utnso
 */

#ifndef FUNCIONESFS_H_
#define FUNCIONESFS_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdint.h>
#include "utils.h"
#include <commons/config.h>
#include <commons/string.h>
#include <string.h>
//#define PUERTO "6667"
#define BACKLOG 5			// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
#define MAX_PACKAGE_SIZE 1024	//El servidor no admitira paquetes de mas de 1024 bytes
#define MAXUSERNAME 30
#define MAX_MESSAGE_SIZE 300
#define TRUE 1


int PUERTO;
//char* PUNTO_MONTAJE;
//char* IP_FILESYSTEM;
char * ARCHCONFIG;

void cargarArchivoDeConfiguracion(char*nombreArchivo);

void procesarMensaje(void * , int ,header);
void procesarComandoConsola(void *,int);

#endif /* FUNCIONESFS_H_ */
