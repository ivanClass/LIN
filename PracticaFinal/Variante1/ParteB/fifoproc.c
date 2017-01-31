#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/semaphore.h>
#include "cbuffer.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FIFO");
MODULE_AUTHOR("Ivan Aguilera Calle & Daniel Garcia Moreno");

#define MAX_ITEMS_CBUF	150
#define MAX_CHARS_KBUF	300


static struct proc_dir_entry *proc_entry;
static int numberFifos = 2;
module_param(numberFifos, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);


typedef struct {
	cbuffer_t* cbuffer;
	int prod_count;
	int cons_count;
	struct semaphore mtx;
	struct semaphore sem_prod;
	struct semaphore sem_cons;
	int nr_prod_waiting;
	int nr_cons_waiting;
} fifo_t;


static fifo_t *fifoArray;

static int fifoproc_open(struct inode *node, struct file *fd){
	fifo_t *private_data  = (fifo_t*)PDE_DATA(fd->f_inode);

	//lock(mtx)
	if(down_interruptible(&private_data->mtx)){
		return -EINTR;
	}

	if(fd->f_mode & FMODE_READ){ //FIFO ABIERTO POR UN CONSUMIDOR
		printk(KERN_INFO "Hay un consumidor\n");
		private_data->cons_count++;

		//cond_signal(sem_prod)
		if(private_data->nr_prod_waiting > 0){
			up(&private_data->sem_prod);
			private_data->nr_prod_waiting--;
			printk(KERN_INFO "Open productor dormido\n");
		}

		//cond_wait(sem_cons)
		while(private_data->prod_count == 0){
			printk(KERN_INFO "Consumidor se duerme porque no hay productor\n");
			private_data->nr_cons_waiting++;
			up(&private_data->mtx);

			if(down_interruptible(&private_data->sem_cons)){
				down(&private_data->mtx);
				private_data->nr_cons_waiting--;
				up(&private_data->mtx);
				return -EINTR;
			}

			if(down_interruptible(&private_data->mtx)){
				return -EINTR;
			}
		}	
	}
	else{ //FIFO ABIERTO POR UN PRODUCTOR
		printk(KERN_INFO "Hay un productor\n");
		private_data->prod_count++;

		//cond_signal(cons);
		if(private_data->nr_cons_waiting > 0){
			up(&private_data->sem_cons);
			private_data->nr_cons_waiting--;
			printk(KERN_INFO "Open consumidor dormido\n");
		}

		//cond_wait(prod)
		while(private_data->cons_count == 0){
			printk(KERN_INFO "Productor se duerme porque no hay consumidor\n");
			private_data->nr_prod_waiting++;
			up(&private_data->mtx);

			if(down_interruptible(&private_data->sem_prod)){
				down(&private_data->mtx);
				private_data->nr_prod_waiting--;
				up(&private_data->mtx);
				return -EINTR;
			}

			if(down_interruptible(&private_data->mtx)){
				return -EINTR;
			}
		}
	}
	//unlock(mtx);
	up(&private_data->mtx);

	return 0;
}

static int fifoproc_release(struct inode *node, struct file *fd){
	fifo_t *private_data  = (fifo_t*)PDE_DATA(fd->f_inode);

	//lock(mtx);
	if(down_interruptible(&private_data->mtx)){
		return -EINTR;
	}
	if(fd->f_mode & FMODE_READ){ //FIFO ABIERTO POR UN CONSUMIDOR
		private_data->cons_count--;

		//cond_signal(sem_prod)
		if(private_data->nr_prod_waiting > 0){
			up(&private_data->sem_prod);
			private_data->nr_prod_waiting--;
			printk(KERN_INFO "Close consumidor\n");
		}		
	}
	else{ //FIFO ABIERTO POR UN PRODUCTOR
		private_data->prod_count--;

		//cond_signal(cons);
		if(private_data->nr_cons_waiting > 0){
			up(&private_data->sem_cons);
			private_data->nr_cons_waiting--;
			printk(KERN_INFO "Close productor\n");
		}
	}

	if((private_data->cons_count + private_data->prod_count) == 0){
		clear_cbuffer_t(private_data->cbuffer);
		printk(KERN_INFO "BUffer reseteado correctamente\n");
	}

	//unlock(mtx);
	up(&private_data->mtx);

	return 0;
}
//fifo_t *private_data  = (fifo_t*)PDE_DATA(fd->f_inode);
static ssize_t fifoproc_read(struct file *fd, char __user *buf, size_t len, loff_t *off){
	fifo_t *private_data = (fifo_t*)PDE_DATA(fd->f_inode);
	
	char kbuffer[MAX_CHARS_KBUF];

	printk(KERN_INFO "El consumidor va a empezar a consumir\n");
	if(len > MAX_ITEMS_CBUF){
		return -1;
	}

	//lock
	if(down_interruptible(&private_data->mtx)){
		return -EINTR;
	}

	printk(KERN_INFO "Elementos en cbuffer que se pueden leer: %d\n", size_cbuffer_t(private_data->cbuffer));
	printk(KERN_INFO "Numero de huecos libres que no se consumen: %d\n", nr_gaps_cbuffer_t(private_data->cbuffer));

	while(size_cbuffer_t(private_data->cbuffer) < len && private_data->prod_count > 0){
		printk(KERN_INFO "El consumidor se duerme. No hay nada que consumir\n");

		//cond_wait(cons, mtx);
		private_data->nr_cons_waiting++;
		up(&private_data->mtx);

		if(down_interruptible(&private_data->sem_cons)){
			down(&private_data->mtx);
			private_data->nr_cons_waiting--;
			up(&private_data->mtx);
			return -EINTR;
		}

		if(down_interruptible(&private_data->mtx)){
			return -EINTR;
		}
	}

	if((private_data->prod_count == 0) && is_empty_cbuffer_t(private_data->cbuffer) != 0){
		//unlock(mtx);
		up(&private_data->mtx);
		return 0;
	}

	printk(KERN_INFO "Numero de elementos antes de remover por consumicion: %d\n", size_cbuffer_t(private_data->cbuffer));
	remove_items_cbuffer_t(private_data->cbuffer, kbuffer, len);
	printk(KERN_INFO "Numero de elementos despues de remover por consumicion: %d\n", size_cbuffer_t(private_data->cbuffer));

	//cond_signal(prod);
	if(private_data->nr_prod_waiting > 0){
		up(&private_data->sem_prod);
		private_data->nr_prod_waiting--;
	}

	//unlock(mtx);
	up(&private_data->mtx);

	if(copy_to_user(buf, kbuffer, len)){ 
		return -1;
	}

	return len;
}

static ssize_t fifoproc_write(struct file *fd, const char *buf, size_t len, loff_t *off){
	fifo_t *private_data = (fifo_t*)PDE_DATA(fd->f_inode);
	
	char kbuffer[MAX_CHARS_KBUF];

	printk(KERN_INFO "El productor va a empezar a producir\n");
	if(len > MAX_ITEMS_CBUF || len > MAX_CHARS_KBUF){ 
		printk(KERN_INFO "Errror en primer if de write\n");
		return -1;
	}

	//BLOQUEAR AL PRODUCTOR SI NO HAY CONSUMIDOR
	
	if(copy_from_user(kbuffer, buf, len)){
		printk(KERN_INFO "Errror en copy_from_user\n");
		return -EFAULT;
	}

	//lock(mtx);
	if(down_interruptible(&private_data->mtx)){
		return -EINTR;
	}

	printk(KERN_INFO "Numero de huecos libres para producir: %d\n", nr_gaps_cbuffer_t(private_data->cbuffer));
	while(nr_gaps_cbuffer_t(private_data->cbuffer) < len && private_data->cons_count > 0){
		printk(KERN_INFO "El productor se duerme. No tengo hueco para producir\n");

		//cond_wait(prod, mtx);
		private_data->nr_prod_waiting++;
		up(&private_data->mtx);

		if(down_interruptible(&private_data->sem_prod)){
			down(&private_data->mtx);
			private_data->nr_prod_waiting--;
			up(&private_data->mtx);
			return -EINTR;
		}

		if(down_interruptible(&private_data->mtx)){
			return -EINTR;
		}

	}

	if(private_data->cons_count == 0){
		//unlock(mtx);
		printk(KERN_INFO "No hay consumidores\n");

		up(&private_data->mtx);
		return -EPIPE;
	}

	printk(KERN_INFO "Numero de elementos antes de insertar por produccion: %d\n", size_cbuffer_t(private_data->cbuffer));
	insert_items_cbuffer_t(private_data->cbuffer, kbuffer, len);
	printk(KERN_INFO "Numero de elementos despues de insertar por produccion: %d\n", size_cbuffer_t(private_data->cbuffer));
	
	//cond_signal(cons);
	if(private_data->nr_cons_waiting > 0){
		up(&private_data->sem_cons);
		private_data->nr_cons_waiting--;
	}

	//unlock(mtx);
	up(&private_data->mtx);

	return len;
}

static const struct file_operations proc_entry_fops = {
    .open =  fifoproc_open,
    .read = fifoproc_read,
    .write = fifoproc_write,
    .release = fifoproc_release
};

int init_module(void){	
	int i = 0;
	char cadena[10];
	fifoArray = (fifo_t*) vmalloc(sizeof(fifo_t) * numberFifos);

	for (i = 0; i < numberFifos; ++i){
		fifoArray[i].cbuffer = create_cbuffer_t(MAX_ITEMS_CBUF);
		if (!fifoArray[i].cbuffer) {
			int j = 0;
			for (j = 0; j < i; ++j){
				destroy_cbuffer_t(fifoArray[j].cbuffer);
				//Borrar todos los  /proc/fifoX
				sprintf(cadena, "fifo%d", j);
				remove_proc_entry(cadena, NULL);
			}
			printk(KERN_INFO "Error al cargar modulo\n");

			vfree(fifoArray);
	   		return -ENOMEM;
		}

		fifoArray[i].prod_count = 0;
		fifoArray[i].cons_count = 0;
		fifoArray[i].nr_prod_waiting = 0;
		fifoArray[i].nr_cons_waiting = 0;

		sema_init(&fifoArray[i].mtx, 1);
		sema_init(&fifoArray[i].sem_prod, 0);
		sema_init(&fifoArray[i].sem_cons, 0); //bloqueado al principio, ya que no hay elementos
		sprintf(cadena, "fifo%d", i);
		proc_entry = proc_create_data(cadena, 0666, NULL, &proc_entry_fops, &fifoArray[i]);

		if (proc_entry == NULL) {
			int j = 0;
			for (j = 0; j < i+1; ++j){
				destroy_cbuffer_t(fifoArray[j].cbuffer);
				//Borrar todos los  /proc/fifoX
				sprintf(cadena, "fifo%d", j);
				remove_proc_entry(cadena, NULL);
			}
			printk(KERN_INFO "Error al cargar modulo\n");

			vfree(fifoArray);
		    return  -ENOMEM;
		}		
	}

	printk(KERN_INFO "Fifomod: Cargado el Modulo con total exito.\n");

	return 0;
}

void cleanup_module(void){
	char cadena[10];
	int i = 0;

	for (i = 0; i < numberFifos; ++i){
		sprintf(cadena, "fifo%d", i);
		remove_proc_entry(cadena, NULL);
		destroy_cbuffer_t(fifoArray[i].cbuffer);
	}

	vfree(fifoArray);
  	printk(KERN_INFO "Fifomod: Modulo descargado con total exito.\n");
}