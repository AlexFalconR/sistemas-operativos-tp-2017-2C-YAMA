#include "funcionesFileSystem.h"

void cargarArchivoDeConfiguracionFS(char* path) {
	char cwd[1024]; // Current Working Directory. Variable donde voy a guardar el path absoluto hasta el /Debug
	char *pathArchConfig = string_from_format("%s/%s", getcwd(cwd, sizeof(cwd)),
			path); // String que va a tener el path absoluto para pasarle al config_create
	t_config *config = config_create(pathArchConfig);
	//log_info(vg_logger, "El directorio sobre el que se esta trabajando es %s.", pathArchConfig);

	if (config_has_property(config, "PUERTO")) {
		PUERTO = config_get_int_value(config, "PUERTO");
	}
}

void* serializarInfoNodo(t_infoNodo* infoNodo, t_header* header) {
	uint32_t bytesACopiar = 0, desplazamiento = 0, largoIp;

	void *payload = malloc(sizeof(uint32_t) * 5); // reservo 4 uint32_t para los primeros 4 campos del struct + 1 para el largo del string ip.

	/* Serializamos el payload */
	bytesACopiar = sizeof(uint32_t);
	memcpy(payload + desplazamiento, &infoNodo->sdNodo, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	memcpy(payload + desplazamiento, &infoNodo->idNodo, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	memcpy(payload + desplazamiento, &infoNodo->cantidadBloques, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	memcpy(payload + desplazamiento, &infoNodo->puerto, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	largoIp = strlen(infoNodo->ip);
	memcpy(payload + desplazamiento, &largoIp, bytesACopiar); // le agrego el largo de la cadena ip como parte del mensaje
	desplazamiento += sizeof(uint32_t);

	payload = realloc(payload, desplazamiento + largoIp); // Hacemos apuntar al nuevo espacio de memoria redimensionado.
	memcpy(payload + desplazamiento, infoNodo->ip, largoIp);
	desplazamiento += largoIp;

	header->tamanioPayload = desplazamiento; // Modificamos por referencia al argumento header.

	/* Serializamos y anteponemos el header */
	void *paquete = malloc(sizeof(uint32_t) * 2 + header->tamanioPayload);
	desplazamiento = 0; // volvemos a empezar..

	bytesACopiar = sizeof(uint32_t);
	memcpy(paquete + desplazamiento, &header->id, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	memcpy(paquete + desplazamiento, &header->tamanioPayload, bytesACopiar);
	desplazamiento += bytesACopiar;

	memcpy(paquete + desplazamiento, payload, header->tamanioPayload);

	return paquete;
}

t_infoNodo deserializarInfoNodo(void* mensaje, int tamanioPayload) {
	t_infoNodo infoNodo;
	uint32_t desplazamiento = 0, bytesACopiar = 0, tamanioIp = 0;

	bytesACopiar = sizeof(uint32_t);
	memcpy(&infoNodo.sdNodo, mensaje + desplazamiento, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	memcpy(&infoNodo.idNodo, mensaje + desplazamiento, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	memcpy(&infoNodo.cantidadBloques, mensaje + desplazamiento, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t);
	memcpy(&infoNodo.puerto, mensaje + desplazamiento, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = sizeof(uint32_t); // recibimos longitud IP
	memcpy(&tamanioIp, mensaje + desplazamiento, bytesACopiar);
	desplazamiento += bytesACopiar;

	bytesACopiar = tamanioIp;
	infoNodo.ip = malloc(tamanioIp + 1);
	memcpy(infoNodo.ip, mensaje + desplazamiento, bytesACopiar);
	infoNodo.ip[tamanioIp] = '\0';
	desplazamiento += bytesACopiar;
	return infoNodo;
}

void * esperarConexionesDatanodes() {
	int socketServidor, numeroClientes = 10, socketsClientes[10] = { }, opt = 1,
			addrlen, max_sd, i, sd, actividad, socketEntrante;
	struct sockaddr_in address;
	fd_set readfds;
	t_header header;
	void *buffer = NULL;

	socketServidor = nuevoSocket();

	// Seteo el socketServidor para permitir multiples conexiones, aunque no sea necesario es una buena practica.
	if (setsockopt(socketServidor, SOL_SOCKET, SO_REUSEADDR, (char *) &opt,
			sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	//Defino el tipo de socket con sus parametros.
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PUERTO);

	// Bind al puerto indicado en la variable PUERTO. Cargada de la config O DEL ARCHIVO HEADER
	if (bind(socketServidor, (struct sockaddr *) &address, sizeof(address))
			< 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	//printf("\nEscuchando en el puerto: %d\n", PUERTO);

	/* Especifica un maximo de BACKLOG conexiones pendientes por parte del socketServidor
	 IMPORTANTE: listen() es una syscall BLOQUEANTE. socketServidor es el que escucha. */
	if (listen(socketServidor, BACKLOG) < 0) {
		perror("Maximo de tres conexiones pendientes");
		exit(EXIT_FAILURE);
	}

	// Acceptar conexiones entrantes
	addrlen = sizeof(address);
	while (1) {
		//Limpio la lista de sockets
		FD_ZERO(&readfds);

		//Agrego el socket master a la lista, para que tambien revise si hay cambios
		FD_SET(socketServidor, &readfds);
		max_sd = socketServidor;

		//add child sockets to set
		for (i = 0; i < numeroClientes; i++) {
			//socket descriptor
			sd = socketsClientes[i];

			//if valid socket descriptor then add to read list
			if (sd > 0)
				FD_SET(sd, &readfds);

			//highest file descriptor number, need it for the select function
			if (sd > max_sd)
				max_sd = sd;
		}

		//Espero que haya actividad en los sockets. Tiempo de espera null, nunca termina.
		actividad = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		//&& (errno!=EINTR))
		if (actividad < 0) {
			printf("select error");
		}

		//Si algo cambio en el socket master, es una conexion entrante
		if (FD_ISSET(socketServidor, &readfds)) {
			if ((socketEntrante = accept(socketServidor,
					(struct sockaddr *) &address, (socklen_t*) &addrlen)) < 0) {
				perror("accept");
				exit(EXIT_FAILURE);
			}

			//inform user of socket number - used in send and receive commands
			printf(
					"Nueva conexion, socket descriptor es: %d , ip es: %s, puerto: %d\n",
					socketEntrante, inet_ntoa(address.sin_addr), PUERTO);

			//Agrego el nuevo socket al array
			for (i = 0; i < numeroClientes; i++) {
				//Busco una pos vacia en la lista de clientes para guardar el socket entrante
				if (socketsClientes[i] == 0) {
					socketsClientes[i] = socketEntrante;
					printf("Se agrego el nuevo socket en la posicion: %d.\n",
							i);
					break;
				}
			}
		}

		//else  es un cambio en los sockets que estaba escuchando.
		for (i = 0; i < numeroClientes; i++) {
			sd = socketsClientes[i];

			if (FD_ISSET(sd, &readfds)) {
				//Chequea si fue para cerrarse y sino lee el mensaje;

				// desconexion
				if (recibirHeader(sd, &header) == 0) {
					//getpeername(sd, (struct sockaddr*) &address, (socklen_t*) &addrlen);
					printf("Cliente desconectado sd: %d \n", sd);
					close(sd);
					socketsClientes[i] = 0;
					//SI SE DESCONECTO UN NODO BUSCARLO EN LA LISTA DE NODOS
					//CONECTADOS (LISTA DE STRUCTS) Y SACARLO.
					//ACTUALIZAR TABLA DE ARCHIVOS Y PASAR BLOQUES DE NODO
					//DESCONECTADO A NO DISPONIBLES
					break;
				}

				//Si entra aca recibi un header que va a tener la info de quien se conecto.
				else {
					printf("Header : %d.\n", header.tamanioPayload);
				}

				buffer = malloc(header.tamanioPayload);

				//Lo que hariamos aca es buscar el nodo en la lista de nodos conectados y sacarlo. Ademas hay que actualizar la tabla de archivos.
				if (recibirPorSocket(sd, buffer, header.tamanioPayload) <= 0) {
					perror(
							"Error. El payload no se pudo recibir correctamente.");
				} else {
					t_infoNodo infoNodo = deserializarInfoNodo(buffer,
							header.tamanioPayload);

					// Crear funcion que maneje la lista de struct t_infoNodo. "add" infoNodo por ejemplo.
					//Tenemos que ver si el hilo de yama entra por aca o ponemos a escuchar en otro hilo aparte
					//SON SOLO PARA DEBUG. COMENTAR Y AGREGAR AL LOGGER.
					printf("***************************************\n");
					printf("sdNodo: %d\n", infoNodo.sdNodo);
					printf("idNodo: %d\n", infoNodo.idNodo);
					printf("cantidadBloques: %d\n", infoNodo.cantidadBloques);
					printf("puerto: %d\n", infoNodo.puerto);
					printf("ip: %s\n", infoNodo.ip);
					//ACTUALIZAR TABLA DE ARCHIVOS Y PASAR A DISPONIBLES LOS
					//BLOQUES DE ESE NODO
				}
				//printf("Paquete recibido. \n Mensaje: %s usuario: %s \n ",nuevoPacketeRecbido.message,nuevoPacketeRecbido.username);
				free(buffer);
			}
		}
	}

	//puts("Esperando conexiones datanode...");

}

//Crea un array de tipo t_bitmap y lo carga al archivo
void cargarArchivoBitmap(FILE* arch, int tamanioDatabin) {
	int i;
	t_bitMap arrayBitmap[tamanioDatabin];
	for (i = 0; i < (tamanioDatabin); i++) {
		arrayBitmap[i].estadoBLoque = 'L';
	}
	for (i = 0; i < (tamanioDatabin); i++) {
		fwrite(&arrayBitmap[i], sizeof(char), 1, arch);
	}
}

int verificarExistenciaArchBitmap(char*nombreArchBitmap, char*path) {
	DIR* directorio;
	directorio = opendir(path);

	if (directorio == NULL) { // Aca se verifica si el directorio a bitmaps existe
		closedir(directorio);
		return 2;
	}
	closedir(directorio);

	if (access(nombreArchBitmap, F_OK) != -1) {
		return 1;
	}
	return 0;

}

// Esta funcion crea un archivo bitmap con el nombre id del nodo y
// tamanio de databin,verifica si existe de antes y si no lo crea.
void crearArchivoBitmapNodo(int idNodo, int tamanioDatabinNodo) {
	char*nombreArchBitmap = armarNombreArchBitmap(idNodo);
	FILE* arch;

	switch (verificarExistenciaArchBitmap(nombreArchBitmap, pathBitmap)) {

	case 0:
		arch = fopen(nombreArchBitmap, "w+");
		cargarArchivoBitmap(arch, tamanioDatabinNodo);
		fclose(arch);
		break;

	case 1:
		printf("Archivo bitmap ya existe");
		break;

	case 2:
		printf("La carpeta bitmaps no existe");  // validacion de carpeta bitmap
		break;
	}

}

//Accede al numero de bloque en el array y modifica su estado
void liberarBloqueBitmapNodo(int numBloque, int idNodo) {
	char* nombreArchBitmap = armarNombreArchBitmap(idNodo);
	FILE* arch;
	t_bitMap regActualizado;
	regActualizado.estadoBLoque = 'L';

	arch = fopen(nombreArchBitmap, "r+");
	fseek(arch, (numBloque - 1), SEEK_SET);
	fwrite(&regActualizado, sizeof(t_bitMap), 1, arch);
	fclose(arch);
}

void ocuparBloqueBitmapNodo(int numBloque, int idNodo) {
	char* nombreArchBitmap = armarNombreArchBitmap(idNodo);
	FILE* arch;
	t_bitMap regActualizado;
	regActualizado.estadoBLoque = 'O';

	arch = fopen(nombreArchBitmap, "r+");
	fseek(arch, (numBloque - 1), SEEK_SET);
	fwrite(&regActualizado, sizeof(t_bitMap), 1, arch);
	fclose(arch);
}

//arma el path nombre del bitmap a partir del id del nodo
char* armarNombreArchBitmap(int idNodo) {
	char* nombreArchBitmap = string_new();
	char* idNodoString = string_itoa(idNodo);
	string_append(&nombreArchBitmap, pathBitmap);
	string_append(&nombreArchBitmap, idNodoString);
	string_append(&nombreArchBitmap, ".dat");

	return nombreArchBitmap;
}

/* Esta funcion, desde el lado del filesystem solamente enviara por socket lo necesario al proceso datanode para que el se ocupe de almacenar.
 * Quedara a la espera de la respuesta de este para mostrar el resultado de la operacion por pantalla.
 */
int almacenarArchivo(char* path, char* nombre, int tipo, char* datos) {

	// 1- planificar nodos, cortar archivos...

	// 2- enviar info al nodo

	// 3- recibir confirmacion (resultado)

	return 0;
}

char* getResultado(int resultado) {
	switch (resultado) {
	case EXITO:
		return "exitoso";
	case ERROR:
		return "erroneo";
	default:
		return "erroneo";
	}
}

t_directory directoriosAGuardar[100] = { { 0, "/", -1 },

};

t_directory directoriosGuardados[100] = { }; // Inicializo el array de estructuras

t_directory directorios[100] = { };

void validarMetadata(char* path) {
	//faltaria sumar aunque sea 1 al malloc por '\0'?
	char *newPath = malloc(strlen(path) + strlen("/metadata"));

	if (newPath) {
		newPath[0] = '\0';
		strcat(newPath, path);
		strcat(newPath, "/metadata");
	} else {
		fprintf(stderr, "malloc fallido!.\n");
	}

	DIR* directoryPointer = opendir(newPath);

	if (directoryPointer) {
		// El directorio existe.
		closedir(directoryPointer);
	}
	// Si el directorio no existe
	else if (ENOENT == errno) {
		mkdir("metadata", 0777); // Le damos todos los permisos, por ahora.
		closedir(directoryPointer);
	}

	free(newPath);
}

void persistirDirectorios(t_directory directorios[], char* path) {
	int i;
	char *newPath = malloc(strlen(path) + strlen("/metadata/directorios.dat"));

	if (newPath != NULL) {
		newPath[0] = '\0';
		strcat(newPath, path);
		strcat(newPath, "/metadata/directorios.dat");
	} else {
		fprintf(stderr, "malloc fallido!.\n");
	}

	FILE *filePointer = fopen(newPath, "w+b");
	fseek(filePointer, 0, SEEK_SET);

	if (!filePointer) {
		fprintf(stderr, "Error. No se puede abrir el archivo");
	}

	for (i = 0; i < 100; i++) {
		fwrite(&directorios[i], sizeof(t_directory), 1, filePointer);
	}

	fclose(filePointer);
	free(newPath);
}

void obtenerDirectorios(t_directory directorios[], char* path) {
	int i;
	char *newPath = malloc(strlen(path) + strlen("/metadata/directorios.dat"));

	if (newPath) {
		newPath[0] = '\0';
		strcat(newPath, path);
		strcat(newPath, "/metadata/directorios.dat");
	} else {
		fprintf(stderr, "malloc fallido!.\n");
	}

	FILE *filePointer = fopen(newPath, "r+b");
	fseek(filePointer, 0, SEEK_SET);

	if (!filePointer) {
		fprintf(stderr, "Error. No se puede abrir el archivo");
	}

	for (i = 0; i < 100; i++) {
		fread(&directorios[i], sizeof(t_directory), 1, filePointer);
	}

	fclose(filePointer);
}

void mostrar(t_directory directorios[]) {
	int i;
	printf("index			nombre						padre\n");
	for (i = 0; i < 100; i++) {
		printf("%d			%s						%d\n", directorios[i].index, directorios[i].nombre,
				directorios[i].padre);
	}
}

int existeDirectorio(char* path, int * padre) {
	int i;

	for (i = 0; i < 100; i++) {
		if ((strcmp(path, directorios[i].nombre) == 0)
				&& (directorios[i].padre = *padre)) {
			*padre = (int) directorios[i].padre;
			return 1;
		}
	}
	return 0;
}

int buscarPrimerLugarLibre() {
	int posicion = 0, i;

	for (i = 0; i < 100; i++) {
		if (strcmp(directorios[i].nombre, "") == 0) {
			posicion = i;
			break;
		}
	}

	return posicion;
}

void mkDirFS(char * path) {
	int coincidencias = 0;
	int padre = 0;
	int i;
	int cantidadPartesPath;
	int posicionLibre = -1;
	char ** pathSeparado;

	pathSeparado = string_split(path, "/");
	cantidadPartesPath = cantidadArgumentos(pathSeparado);
	for (i = 0; (i < cantidadPartesPath); i++) {
		if (existeDirectorio(pathSeparado[i], &padre))
			coincidencias++;
		else
			break;
	}

	switch (coincidencias - cantidadPartesPath) {
	case 0:

		printf("El directorio ya existe");
		break;

	case 1:
		posicionLibre = buscarPrimerLugarLibre();
		if (posicionLibre != -1) {
			crearDirectorioLogico(pathSeparado[cantidadPartesPath - 1], padre,
					posicionLibre);
			crearDirectorioFisico(posicionLibre);
		} else
			printf("La tabla de directorios esta completa");
		break;

	default:
		printf("El directorio no se puede crear. La ruta no existe");
	}
}

void crearDirectorioLogico(char* nombre, int padre, int indice) {
	directorios[indice].index = indice;
	cargarNombre(nombre, indice);
	directorios[indice].padre = padre;
}

void cargarNombre(char* name, int indice) {
	int i = 0;
	while (name[i] != '\0') {
		directorios[indice].nombre[i] = name[i];
		i++;
	}

	directorios[indice].nombre[i] = '\0';
}

void crearDirectorioFisico(int indice) {
	char* path = string_itoa(indice);
	char *newPath = malloc(strlen(path) + strlen("/metadata/archivos/") + 1);

	if (newPath) {
		newPath = "/metadata/archivos/";
		strcat(newPath, path);
	} else {
		fprintf(stderr, "malloc fallido!.\n");
	}

	DIR* directoryPointer = opendir(newPath);

	if (directoryPointer) {
		// El directorio existe.
		closedir(directoryPointer);
	}
	// Si el directorio no existe
	else if (ENOENT == errno) {
		mkdir(path, 0777); // Le damos todos los permisos, por ahora.
		closedir(directoryPointer);
	}

	free(newPath);
}
