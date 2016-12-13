obj-m = kfifomod.o #kfifomod.c no ha de existir
kfifomod-objs = kfifoproc.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean