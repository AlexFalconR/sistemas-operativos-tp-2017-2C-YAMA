#include "funcionesDataNode.h"

int main(void) {
	int socketFs;

	NODOARCHCONFIG = "nodoConfig.cfg";
	cargarArchivoConfiguracionDatanode(NODOARCHCONFIG);
	abrirDatabin();

	socketFs = conectarAfilesystem(IP_FILESYSTEM, PUERTO_FILESYSTEM);
	if (socketFs == 0) {
		perror("No se pudo conectar al FileSystem.");
	} else {
		printf("Conectado al FileSystem con socket n°: %d.\n", socketFs);
	}

	// Escuchar peticiones FileSystem (get y set)
	if (socketFs != 0) {
		cerrarSocket(socketFs);
	}

	cerrarDatabin();
	return 0;
}
