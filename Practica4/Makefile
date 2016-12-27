obj-m = mod_timer.o #modtimer.c no ha de existir
mod_timer-objs = modtimer.o cbuffer.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
