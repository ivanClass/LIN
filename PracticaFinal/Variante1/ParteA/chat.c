#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>

#define MAX_CHARS_MSG 128
#define KNRM  "\x1B[1;0m"
#define KRED  "\x1B[1;31m"
#define KGRN  "\x1B[1;32m"
#define KYEL  "\x1B[1;33m"
#define KBLU  "\x1B[1;34m"
#define KMAG  "\x1B[1;35m"
#define KCYN  "\x1B[1;36m"
#define KWHT  "\x1B[37m"

// http://cc.byexamples.com/2007/01/20/print-color-string-without-ncurses/

typedef enum {
	NORMAL_MSG, /* Mensaje para transferir lineas de la conversacion entre
				ambos usuarios del chat */
	USERNAME_MSG, /* Tipo de mensaje reservado para enviar el nombre de
					usuario al otro extremo*/
	END_MSG /* Tipo de mensaje que se envía por el FIFO cuando un extremo
			finaliza la comunicación */
}message_type_t;

struct chat_message{
	char contenido[MAX_CHARS_MSG]; //Cadena de caracteres (acabada en '\0)
	message_type_t type;
};

struct argsThread {
	char *path_fifo_envio;
	char *path_fifo_recepcion;
	char *username;
};

//argv[1]->Nombre usuario
//argv[2]->FIFO ENVIO
//argv[3]->FIFO RECEPCIÓN
pthread_t threadEmisor;
pthread_t threadReceptor;

void compruebaFallosWrite(int wbytes, int size){
	if (wbytes > 0 && wbytes!=size) {
		fprintf(stderr,"Can't write the whole register\n");
		exit(1);
	}
	else if (wbytes < 0){
		perror("Error when writing to the FIFO\n");
		exit(1);
	}	
}

void compruebaFallosOpen(int fd_fifo, char *path_fifo){
	if (fd_fifo < 0) {
		perror(path_fifo);
		exit(1);
	}
}

//FUNCION EMISOR
void* envioMensajes(void *arg){
	struct argsThread *args = arg;
	char *usuario = args->username;
	char *path_fifo = args->path_fifo_envio;

	struct chat_message message;
	int fd_fifo = 0;
	int bytes = 0, wbytes = 0;
	const int size = sizeof(struct chat_message);

	//ABRIR FIFO MODO ESCRITURA
	fd_fifo = open(path_fifo, O_WRONLY);
	compruebaFallosOpen(fd_fifo, path_fifo);
	printf("%sConexión de envío establecida!!%s\n", KGRN, KNRM);

	//ENVIAR NOMBRE DE USUARIO AL OTRO EXTREMO USANDO MENSAJE TIPO USERNAME_MSG
	sprintf(message.contenido, "%s", usuario);
	message.type = USERNAME_MSG;
	wbytes = write(fd_fifo, &message, size);
	memset(message.contenido, 0, sizeof(message.contenido));
	compruebaFallosWrite(wbytes, size);	

	////LEE LAS LINEAS DE LA ENTRADA ESTÁNDAR UNA A UNA Y LAS ENVÍA ENCAPSULADAS EN EL
	////CAMPO CONTENIDO DEL MENSAJE NORMAL_MESSAGE AL OTRO EXTREMO
	/* Bucle de envío de datos a través del FIFO
	- Leer de la entrada estandar hasta fin de fichero
	*/
	while((bytes = read(0, message.contenido, MAX_CHARS_MSG)) > 0) {
		//message.nr_bytes=bytes;
		message.type = NORMAL_MSG;

		wbytes = write(fd_fifo, &message, size);
		memset(message.contenido, 0, sizeof(message.contenido));
		compruebaFallosWrite(wbytes, size);	
	}

	if (bytes < 0) {
		fprintf(stderr,"Error when reading from stdin\n");
		exit(1);
	}

	////EL PROCESAMIENTO FINALIZA CUANDO EL USUARIO TECLEA CTRL+D -> SE ENVIA END_MESSAGE
	message.type = END_MSG;
	wbytes = write(fd_fifo, &message, size);
	compruebaFallosWrite(wbytes, size);		

	////CERRAR FIFO DE ENVIO Y SALIR
	close(fd_fifo);
	exit(0);
}

//FUNCION RECEPTOR
void* recepcionMensajes(void *arg){
	struct argsThread *args = arg;
	char usuarioEmisor[10];
	char *path_fifo = args->path_fifo_recepcion;
	char miCadena[MAX_CHARS_MSG];

	struct chat_message message;
	int fd_fifo = 0;
	int bytes = 0, wbytes = 0;
	const int size = sizeof(struct chat_message);

	////ABRIR FIFO MODO LECTURA
	fd_fifo = open(path_fifo, O_RDONLY);
	compruebaFallosOpen(fd_fifo, path_fifo);
	printf("%sConexión de recepción establecida!!%s\n", KGRN, KNRM);

	while((bytes = read(fd_fifo, &message, size)) == size && message.type != END_MSG) {
		//PROCESA MENSAJE DE NOMBRE DE USUARIO USERNAME_MSG LEYENDO FIFO DE RECEPCIÓN
		if(message.type == USERNAME_MSG){
			sprintf(usuarioEmisor, "%s", message.contenido);
		}
		else if(message.type == NORMAL_MSG){
			sprintf(miCadena, "%s%s dice:%s %s%s", KBLU, usuarioEmisor, KCYN, message.contenido, KNRM);
			wbytes = write(1, miCadena, strlen(miCadena));
			if (wbytes != strlen(miCadena)) {
				fprintf(stderr,"Can't write data to stdout\n");
				exit(1);
			}	
			memset(miCadena, 0, sizeof(miCadena));			
		}

	}

	if(message.type == END_MSG){
		printf("%sConexión finalizada por %s%s\n", KRED, usuarioEmisor, KNRM);
	}
	else if (bytes > 0){
		fprintf(stderr,"Can't read the whole register\n");
		exit(1);
	}
	
	else if (bytes < 0) {
		fprintf(stderr,"Error when reading from the FIFO\n");
		exit(1);
	}

	////CIERRA FIFO DE RECEPCION Y SALE
	close(fd_fifo);
	exit(0);
}

int main(int argc, char **argv){
	int i = 0;
	int err;

	struct argsThread misArgs;

	if(argc != 4){
		printf("Uso Incorrecto!: <usuario> <fifo-envio> <fifo-recepcion>\n");
		exit(1);
	}

	misArgs.username = argv[1];
	misArgs.path_fifo_envio = argv[2];
	misArgs.path_fifo_recepcion = argv[3];

	//Thread emisor
	err = pthread_create(&threadEmisor, NULL, &envioMensajes, &misArgs);
    if (err != 0)
        printf("\ncan't create thread :[%s]", strerror(err));
    
    //Thread recepción
	err = pthread_create(&threadReceptor, NULL, &recepcionMensajes, &misArgs);
    if (err != 0)
        printf("\ncan't create thread :[%s]", strerror(err));

    pthread_join(threadEmisor, NULL);
    pthread_join(threadReceptor, NULL);
    

	return 0;
}