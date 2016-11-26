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

#ifdef __i386__
#define __NR_LEDCTL    353
#else
#define __NR_LEDCTL    316
#endif

long ledctl(unsigned int leds){
	return (long) syscall(__NR_LEDCTL, leds);
}

void muestrameHoraLeds();
void enciendeLeds(int nVecesEnciende, unsigned int cadena);
void codificaBinario();


int main(){

	int salir = 0;
	int opcion;
	ledctl(0);

	while(salir == 0){
		printf("1.- Muestra la hora en los leds\n");
		printf("2.- Codifica en binario (valores 0-7)\n");
		printf("0.- Salir\n");
		printf("Introduce opción (0, 1, 2): ");

		scanf("%d", &opcion);
		if(opcion > 0 && opcion < 3 ){
			switch (opcion) {
				case 1:
					muestrameHoraLeds();
					break;
				case 2:
					codificaBinario();
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


void muestrameHoraLeds(){

	time_t cosa1 = time(0);
	struct tm *cosa2 = localtime(&cosa1);

	int horas = cosa2->tm_hour;
	int minutos = cosa2->tm_min;
	int segundos = cosa2->tm_sec;

	printf("Son las: %d:%d:%d horas\n",horas,minutos,segundos);

	
	//1--> Parpadea el led numlock tantas veces como la hora que sea
	enciendeLeds(horas, 0x4);
	//2--> Parpadea el led capslock tantas veces como el minuto que sea
	enciendeLeds(minutos, 0x2);
	//3--> Parpadea el led scrolllock tantas veces como el segundo que sea
	enciendeLeds(segundos, 0x1);

	//FINALIZACIÓN
	enciendeLeds(5, 0x7);
}

void enciendeLeds(int nVecesEnciende, unsigned int leds){
	int i;

	struct timespec tiempo;
	tiempo.tv_sec = 700 / 1000;
	tiempo.tv_nsec = (700 % 1000) * 1000000;

	for (i = 0; i < nVecesEnciende; ++i) {
		ledctl(leds);
		nanosleep(&tiempo, NULL);
		ledctl(0x0); //Escribimos un 0 para que el led parpadee
		nanosleep(&tiempo, NULL);
	}
}

void codificaBinario(){
	unsigned int ndecimal;

	printf("Introduce número a codificar (0-7): ");

	scanf("%x", &ndecimal);
	ledctl(ndecimal);
}
