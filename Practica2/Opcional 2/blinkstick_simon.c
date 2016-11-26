#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <linux/errno.h>
#include <sys/syscall.h>
#include <linux/unistd.h>

#define MAXSIZE 8
#define clear() printf("\033[H\033[J")

static int MAXINTENTOS = 3;
static const char todosRojos[] = "0:0x3A0000, 1:0x3A0000, 2:0x3A0000, 3:0x3A0000, 4:0x3A0000, 5:0x3A0000, 6:0x3A0000, 7:0x3A0000";
static const char todosVerdes[] = "0:0x001000, 1:0x001000, 2:0x001000, 3:0x001000, 4:0x001000, 5:0x001000, 6:0x001000, 7:0x001000";

void simon();
void parpadeo(char *cadena, int numVeces, int filedesc);
void apagarLeds(int filedesc);
void cambiarIntentos();


int main(){

	int salir = 0;
	int opcion;
	clear();
	while(salir == 0){
		printf("1.- Jugar\n");
		printf("2.- Cambiar numero de intentos\n");
		printf("0.- Salir\n");
		printf("Introduce opción (0, 1, 2): ");

		scanf("%d", &opcion);
		if(opcion > 0 && opcion < 3 ){
			switch (opcion) {
				case 1:
					getchar();
					simon();
					break;
				case 2:
					getchar();
					cambiarIntentos();
					break;
				default:
					break;
			}
		}
		if(opcion == 0){
			salir = 1;
			printf("Hasta pronto! :)\n");

		}
	}

	return 0;
}

#define NR_SAMPLE_COLORS

unsigned int colors[]={0x001000, 0x00006F, 0x3A0000, 0x1F2000, 0x551111, 0xDD9000, 0x33CC66, 0x613361}; //verde,azul,rojo,amarillo,carne,naranaja,azul-turquesa,lila 

void simon(){
	char *cadena;
	char comandoJugador[20];
	//0-> numero libre, 1->numero ocupado
	int numOcupadoRandom[MAXSIZE] = {0};
	int secuencia[MAXSIZE];
	int filedesc;
	int jugada = 0;
	int mec;
	int repetido = 1;
	int fallo = 0;  //2 intentos por partida
	int confundido = 0; //si se ha confundido en una posible secuencia
	int posibleLed;
	int formatoBien = 1;

	filedesc = open("/dev/usb/blinkstick0", O_RDWR|O_TRUNC);
	apagarLeds(filedesc);
	if(filedesc < 0)
    	return;

	srand(time(NULL));
	clear();

	for(mec = 0; mec < MAXSIZE; mec++){
		while(repetido){
			secuencia[mec] = rand() % MAXSIZE;
			if(numOcupadoRandom[secuencia[mec]] != 1){
				numOcupadoRandom[secuencia[mec]] = 1;
				repetido = 0;
			}

		}

		repetido = 1;
	}



	cadena = (char*) malloc (12);

	while(fallo < MAXINTENTOS && jugada < MAXSIZE){

		int aux;

		//PENDIENTE POR HACERR -> TO DO
		//COntrol de errores.

		for(mec = 3; mec > 0; mec--){
			printf("Empieza en %d:\n", mec);
			sleep(1);
			clear();
		}
		printf("¡¡¡¡¡Ya!!!!!\n");
		

        for(aux = 0; aux <= jugada; aux++){
        	sprintf(cadena, "%d:%x", secuencia[aux], colors[secuencia[aux]]);
        	parpadeo(cadena, 1, filedesc);
        }
		clear();
        printf("Inserte números del 1-8 separados por espacio. Ej: <1 2 3>: ");
        fgets(comandoJugador, 20, stdin);
	
        aux = 0;
        confundido = 0;
        while(!confundido && aux <= jugada){
        	//formatoBien = sscanf(comandoJugador[aux*2], "%d", &posibleLed);
        	posibleLed = comandoJugador[aux*2] - '0'; //comandoJugador tiene un valor numérico (aunk sea una letra) menos el valor num. del caracter 0 nos da el numero
        	posibleLed--; //El usuario mete números del 1-8
        	if(posibleLed != secuencia[aux]){
        		confundido = 1;
        	}

        	aux++;
        }

        if(confundido){
    		fallo++;

			if(fallo == MAXINTENTOS)
				printf("Oooooh!!!!, has perdido :( \n");
			else if(fallo >= 1)
	        	printf("Se ha confundido :( \n Tiene otra oportunidad\n");

            parpadeo(todosRojos, 3, filedesc);
        }
        else{
        	parpadeo(todosVerdes, 3, filedesc);
            jugada++;
        }
        clear();
	}
	if(fallo < MAXINTENTOS){
		printf("Enhorabuena!!!!, has ganado :) \n");
	}

	free(cadena);
	filedesc = close(filedesc);
    if(filedesc < 0)
        return;
}

void parpadeo(char *cadena, int numVeces, int filedesc){
	int i;
	struct timespec tiempo;
	tiempo.tv_sec = 400 / 1000;
	tiempo.tv_nsec = (400 % 1000) * 1000000;

	for(i = numVeces; i > 0; i--){
        write(filedesc, cadena, strlen(cadena)*sizeof(char));
		nanosleep(&tiempo, NULL);
		apagarLeds(filedesc);
        nanosleep(&tiempo, NULL);
	}
}

void apagarLeds(int filedesc){
	write(filedesc, " ", strlen(" ")*sizeof(char));
}

void cambiarIntentos(){
	int numIntentos;

	printf("Introduce nuevo número de intentos: ");
	if(scanf("%d", &numIntentos) == 1 && numIntentos > 0){
		MAXINTENTOS = numIntentos;
		printf("Número de intentos actualizado: %d \n",MAXINTENTOS);
		sleep(1);
	}
	else{
		printf("Valor no válido\n");
		sleep(1);

	}
	getchar();
	clear();
}
