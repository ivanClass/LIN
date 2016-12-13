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

#define MAX_ITEMS_CBUF	50
#define MAX_CHARS_KBUF	100

cbuffer_t* cbuffer;
int prod_count = 0;
int cons_count = 0;
struct semaphore mtx;
struct semaphore sem_prod;
struct semaphore sem_cons;
int nr_prod_waiting = 0;
int nr_cons_waiting = 0;
static struct proc_dir_entry *proc_entry;

static int fifoproc_open(struct inode *node, struct file *fd){
	//lock(mtx)
	if(down_interruptible(&mtx)){
		return -EINTR;
	}

	if(fd->f_mode & FMODE_READ){ //FIFO ABIERTO POR UN CONSUMIDOR
		printk(KERN_INFO "Hay un consumidor\n");
		cons_count++;

		//cond_signal(sem_prod)
		if(nr_prod_waiting > 0){
			up(&sem_prod);
			nr_prod_waiting--;
			printk(KERN_INFO "Open productor dormido\n");
		}

		//cond_wait(sem_cons)
		while(prod_count == 0){
			printk(KERN_INFO "Consumidor se duerme porque no hay productor\n");
			nr_cons_waiting++;
			up(&mtx);

			if(down_interruptible(&sem_cons)){
				down(&mtx);
				nr_cons_waiting--;
				up(&mtx);
				return -EINTR;
			}

			if(down_interruptible(&mtx)){
				return -EINTR;
			}
		}	
	}
	else{ //FIFO ABIERTO POR UN PRODUCTOR
		printk(KERN_INFO "Hay un productor\n");
		prod_count++;

		//cond_signal(cons);
		if(nr_cons_waiting > 0){
			up(&sem_cons);
			nr_cons_waiting--;
			printk(KERN_INFO "Open consumidor dormido\n");
		}

		//cond_wait(prod)
		while(cons_count == 0){
			printk(KERN_INFO "Productor se duerme porque no hay consumidor\n");
			nr_prod_waiting++;
			up(&mtx);

			if(down_interruptible(&sem_prod)){
				down(&mtx);
				nr_prod_waiting--;
				up(&mtx);
				return -EINTR;
			}

			if(down_interruptible(&mtx)){
				return -EINTR;
			}
		}
	}
	//unlock(mtx);
	up(&mtx);

	return 0;
}

static int fifoproc_release(struct inode *node, struct file *fd){
	//lock(mtx);
	if(down_interruptible(&mtx)){
		return -EINTR;
	}
	if(fd->f_mode & FMODE_READ){ //FIFO ABIERTO POR UN CONSUMIDOR
		cons_count--;

		//cond_signal(sem_prod)
		if(nr_prod_waiting > 0){
			up(&sem_prod);
			nr_prod_waiting--;
			printk(KERN_INFO "Close consumidor\n");
		}		
	}
	else{ //FIFO ABIERTO POR UN PRODUCTOR
		prod_count--;

		//cond_signal(cons);
		if(nr_cons_waiting > 0){
			up(&sem_cons);
			nr_cons_waiting--;
			printk(KERN_INFO "Close productor\n");
		}
	}

	if(cons_count + prod_count == 0){
		clear_cbuffer_t(cbuffer);
	}

	//unlock(mtx);
	up(&mtx);

	return 0;
}

static ssize_t fifoproc_read(struct file *fd, char __user *buf, size_t len, loff_t *off){
	char kbuffer[MAX_CHARS_KBUF];

	printk(KERN_INFO "El consumidor va a empezar a consumir\n");
	if(len > MAX_ITEMS_CBUF){
		return -1;
	}

	//lock
	if(down_interruptible(&mtx)){
		return -EINTR;
	}

	printk(KERN_INFO "Elementos en cbuffer que se pueden leer: %d\n", size_cbuffer_t(cbuffer));
	printk(KERN_INFO "Numero de huecos libres que no se consumen: %d\n", nr_gaps_cbuffer_t(cbuffer));

	while(size_cbuffer_t(cbuffer) < len && prod_count > 0){
		printk(KERN_INFO "El consumidor se duerme. No hay nada que consumir\n");

		//cond_wait(cons, mtx);
		nr_cons_waiting++;
		up(&mtx);

		if(down_interruptible(&sem_cons)){
			down(&mtx);
			nr_cons_waiting--;
			up(&mtx);
			return -EINTR;
		}

		if(down_interruptible(&mtx)){
			return -EINTR;
		}
	}

	if(prod_count == 0){
		//unlock(mtx);
		up(&mtx);
		return 0;
	}

	printk(KERN_INFO "Numero de elementos antes de remover por consumicion: %d\n", size_cbuffer_t(cbuffer));
	remove_items_cbuffer_t(cbuffer, kbuffer, len);
	printk(KERN_INFO "Numero de elementos despues de remover por consumicion: %d\n", size_cbuffer_t(cbuffer));

	//cond_signal(prod);
	if(nr_prod_waiting > 0){
		up(&sem_prod);
		nr_prod_waiting--;
	}

	//unlock(mtx);
	up(&mtx);

	if(copy_to_user(buf, kbuffer, len)){ 
		return -1;
	}

	return len;
}

static ssize_t fifoproc_write(struct file *fd, const char *buf, size_t len, loff_t *off){
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
	if(down_interruptible(&mtx)){
		return -EINTR;
	}

	printk(KERN_INFO "Numero de huecos libres para producir: %d\n", nr_gaps_cbuffer_t(cbuffer));
	while(nr_gaps_cbuffer_t(cbuffer) < len && cons_count > 0){
		printk(KERN_INFO "El productor se duerme. No tengo hueco para producir\n");

		//cond_wait(prod, mtx);
		nr_prod_waiting++;
		up(&mtx);

		if(down_interruptible(&sem_prod)){
			down(&mtx);
			nr_prod_waiting--;
			up(&mtx);
			return -EINTR;
		}

		if(down_interruptible(&mtx)){
			return -EINTR;
		}

	}

	if(cons_count == 0){
		//unlock(mtx);
		printk(KERN_INFO "No hay consumidores\n");

		up(&mtx);
		return -EPIPE;
	}

	printk(KERN_INFO "Numero de elementos antes de insertar por produccion: %d\n", size_cbuffer_t(cbuffer));
	insert_items_cbuffer_t(cbuffer, kbuffer, len);
	printk(KERN_INFO "Numero de elementos despues de insertar por produccion: %d\n", size_cbuffer_t(cbuffer));
	
	//cond_signal(cons);
	if(nr_cons_waiting > 0){
		up(&sem_cons);
		nr_cons_waiting--;
	}

	//unlock(mtx);
	up(&mtx);

	return len;
}

static const struct file_operations proc_entry_fops = {
    .read = fifoproc_read,
    .write = fifoproc_write,
    .open =  fifoproc_open,
    .release = fifoproc_release
};

int init_module(void){
	cbuffer = create_cbuffer_t(MAX_ITEMS_CBUF);
	if (!cbuffer) {
	   	return -ENOMEM;
	}
	  
	sema_init(&mtx, 1);
	sema_init(&sem_prod, 0);
	sema_init(&sem_cons, 0); //bloqueado al principio, ya que no hay elementos

	proc_entry = proc_create_data("fifomod", 0666, NULL, &proc_entry_fops, NULL); 
	if (proc_entry == NULL) {
	    destroy_cbuffer_t(cbuffer);
	    printk(KERN_INFO "Fifomod: No puedo crear la entrada en proc\n");
	    return  -ENOMEM;
	}
	      
	printk(KERN_INFO "Fifomod: Cargado el Modulo con total exito.\n");
	  
	return 0;

}

void cleanup_module(void){
	remove_proc_entry("fifomod", NULL);
  	destroy_cbuffer_t(cbuffer);
  	printk(KERN_INFO "Fifomod: Modulo descargado con total exito.\n");
}