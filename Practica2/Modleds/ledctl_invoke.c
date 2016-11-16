#include <linux/errno.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <stdio.h>
#ifdef __i386__
#define __NR_LEDCTL    353
#else
#define __NR_LEDCTL    316
#endif

long ledctl(unsigned int leds){
	return (long) syscall(__NR_LEDCTL, leds);
}

int main(int argc, char *argv[]){
	unsigned int i;
	int ret = 0;

	if(argc >= 2){
		if(sscanf(argv[1],"%x", &i) == 1){
			ret = ledctl(i);

			//GUARDAR VALOR RETORNO DE ledctl
			if(ret == -1)
				perror("Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		}
	}

	return ret;
}
