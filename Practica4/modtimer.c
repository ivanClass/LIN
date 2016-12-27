#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/random.h>
#include "cbuffer.h"
#include <asm-generic/errno.h>
#include <asm-generic/uaccess.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ModTimerModule Module");
MODULE_AUTHOR("Ivan Aguilera Calle & Daniel García Moreno");

#define MAX_ITEMS_CBUF	50
#define MAX_CHARS_KBUF	100

struct timer_list my_timer; /* Structure that describes the kernel timer */
cbuffer_t* cbuffer;
static struct proc_dir_entry *proc_entry_modtimer;
static struct proc_dir_entry *proc_entry_modconfig;

//VARIABLES DE CONFIGURACION
static int timer_period_ms;
static int emergency_treshold;
static int max_random;


/* Function invoked when timer expires (fires) */
static void fire_timer(unsigned long data)
{
	//GENERAR NUÜMERO ALEATORIO
        
    /* Re-activate the timer one second from now */
	mod_timer( &(my_timer), jiffies + HZ); //jiffies + HZ = 1s
}

static ssize_t modconfig_proc_read(struct file *fd, char __user *buf, size_t len, loff_t *off){
	char *dest;
	char *kbuff;

	kbuff = (char *)vmalloc(len);
	
	if ((*off) > 0){ /* Tell the application that there is nothing left to read */
	  vfree(kbuff);
	  return 0;
	}

	dest = kbuff;

	dest += sprintf(kbuff, "timer_period_ms = %i\nemergency_treshold = %i\nmax_random = %i\n", timer_period_ms, emergency_treshold, max_random);

	/* Transfer data from the kernel to userspace */  
	if (copy_to_user(buf, &kbuff[0], dest - kbuff)){
		vfree(kbuff);
		return -EINVAL;
	}

	(*off) += (dest-kbuff);  /* Update the file pointer */
	
	return (dest-kbuff);
}

static ssize_t modconfig_proc_write(struct file *fd, const char *buf, size_t len, loff_t *off){
	char *kbuff;


	kbuff = (char *)vmalloc(len);

	if ((*off) > 0){ 
		vfree(kbuff);/* The application can write in this entry just once !! */
		return 0;
	}

	/* Transfer data from user to kernel space */
	if (copy_from_user(&kbuff[0], buf, len )){
		vfree(kbuff);
		return -EFAULT;
	} 

	kbuff[len] = '\0';

	if(sscanf(kbuff, "timer_period_ms %d", &timer_period_ms) == 1) {
		printk(KERN_INFO "Nuevo valor de timer_period_ms: %d\n", timer_period_ms);
		
	}
	else if (sscanf(kbuff, "emergency_treshold %d", &emergency_treshold) == 1){
		printk(KERN_INFO "Nuevo valor de emergency_treshold: %d\n", emergency_treshold);	
	}
	else if (sscanf(kbuff, "max_random %d", &max_random) == 1){
		printk(KERN_INFO "Nuevo valor de max_random %d\n", max_random);	
	}


	*off+=len;            /* Update the file pointer */
	vfree(kbuff);

	return len;
}

//MODTIMER///////////////////////////////////////////////////////////////////////////
static ssize_t modtimer_proc_read(struct file *fd, char __user *buf, size_t len, loff_t *off){
	return 0;
}

static int modtimer_proc_open(struct inode *node, struct file *fd){
	return 0;
}

static int modtimer_proc_release(struct inode *node, struct file *fd){
	return 0;
}


//OPERATIONS STRUCTS/////////////////////////////////////////////////////////////////
static const struct file_operations proc_entry_fops_modtimer = {
    .read = modtimer_proc_read,
    .open = modtimer_proc_open,
    .release = modtimer_proc_release
};

static const struct file_operations proc_entry_fops_modconfig = {
    .read = modconfig_proc_read,
    .write = modconfig_proc_write
};

int init_modtimer_module( void )
{
    //TEMPORIZADOR///////////////////////////////////////////////////////////////////
    /* Create timer */
    init_timer(&my_timer);
    /* Initialize field */
    my_timer.data = 0;
    my_timer.function = fire_timer;
    my_timer.expires = jiffies + HZ;  /* Activate it one second from now */


    //BUFFER CIRCULAR/////////////////////////////////////////////////////////////////
	cbuffer = create_cbuffer_t(MAX_ITEMS_CBUF);
	if (!cbuffer) {
	   	return -ENOMEM;
	}
	  
	/*sema_init(&mtx, 1);
	sema_init(&sem_prod, 0);
	sema_init(&sem_cons, 0); //bloqueado al principio, ya que no hay elementos
	*/

	//ENTRADA PROC MODTIMER///////////////////////////////////////////////////////////
	proc_entry_modtimer = proc_create_data("modtimer", 0666, NULL, &proc_entry_fops_modtimer, NULL); 
	if (proc_entry_modtimer == NULL) {
	    destroy_cbuffer_t(cbuffer);
	    printk(KERN_INFO "ModTimer: No puedo crear la entrada en proc\n");
	    return  -ENOMEM;
	}
	//ENTRADA PROC MODCONFIG//////////////////////////////////////////////////////////
	proc_entry_modconfig = proc_create_data("modconfig", 0666, NULL, &proc_entry_fops_modconfig, NULL); 
	if (proc_entry_modconfig == NULL) {
	    destroy_cbuffer_t(cbuffer);
	    printk(KERN_INFO "ModConfig: No puedo crear la entrada en proc\n");
	    return  -ENOMEM;
	}

	//INICIALIZAR VARIABLES DE CONFIGURACIÓN//////////////////////////////////////////
	timer_period_ms = 500;
	emergency_treshold = 80;
	max_random = 300;

	      
	printk(KERN_INFO "ModTimer: Cargado el Modulo con total exito.\n");
	printk(KERN_INFO "ModConfig: Cargado el Modulo con total exito.\n");
	  
	//EMPIEZA LA CUENTA///////////////////////////////////////////////////////////////
	/* Activate the timer for the first time */
    add_timer(&my_timer);

    return 0;
}


void cleanup_modtimer_module( void ){
  /* Wait until completion of the timer function (if it's currently running) and delete timer */
  del_timer_sync(&my_timer);

  remove_proc_entry("modtimer", NULL);
  remove_proc_entry("modconfig", NULL);
  destroy_cbuffer_t(cbuffer);
  printk(KERN_INFO "ModTimer: Modulo descargado con total exito.\n");
  printk(KERN_INFO "ModConfig: Modulo descargado con total exito.\n");

  //CONTROLAR QUE CUANDO HAYA UN PROCESO QUE ESTË ESCRIBIENDO NO SE PUEDA DESCARGAR
}

module_init( init_modtimer_module );
module_exit( cleanup_modtimer_module );
