INSTALACIÓN

make clean

make

sudo insmod fifomod.ko [numberFifos = X] //X es el número de /proc/fifoX que queremos crear

USO

gcc -pthread -o chat chat.c //Para compilar el programa del chat

TERMINAL 1

./chat <Nombre usuario> /proc/fifo0 /proc/fifo1 --> escribe en fifo0 y recibe por fifo1

TERMINAL 2

./chat <Nombre usuario 2> /proc/fifo1 /proc/fifo0 --> escribe en fifo1 y recibe por fifo0

A partir de este punto pueden comenzar a mandarse mensajes

FINALIZACIÓN DE LA COMUNICACIÓN
CTRL + D


DESINSTALACION
sudo rmmod fifomod


PARTE B


COMPILACION PARA ANDROID X86


gcc -m32 -static -lpthread chat.c -o chat-android