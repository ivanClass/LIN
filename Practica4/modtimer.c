#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/vmalloc.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <asm-generic/errno.h>
#include <asm-generic/uaccess.h>
#include "cbuffer.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ModTimerModule Module");
MODULE_AUTHOR("Ivan Aguilera Calle & Daniel García Moreno");

#define MAX_ITEMS_CBUF	64
#define MAX_CHARS_KBUF	100

struct timer_list my_timer; /* Structure that describes the kernel timer */
cbuffer_t* cbuffer;
static struct proc_dir_entry *proc_entry_modtimer;
static struct proc_dir_entry *proc_entry_modconfig;

//LISTA ENLAZADA
struct list_head mylist;
typedef struct {
	int data;
	struct list_head links;
} list_item_t;

//VARIABLES DE CONFIGURACION
static int timer_period_ms;
static int emergency_treshold;
static int max_random;

//SPINLOCK
DEFINE_SPINLOCK(sp);

//SEMAFOROS
struct semaphore mtx;           //Semaforo que se encarga de la sincronizacion de los procesos que modifican la lista
struct semaphore mtx2;          //Semaforo que se encarga de la sincronizacion de los procesos que quieren leer.
int procesosWaitingRead = 0;

/* Work descriptor */
struct work_struct transfer_task;


//Function invoked when timer expires (fires)////////////////////////////////////////
static void fire_timer(unsigned long data)
{
	unsigned int numAleatorio;
	int cpuActual;
	unsigned long flags;

	numAleatorio = get_random_int() % max_random;

	spin_lock_irqsave(&sp, flags);
		
		if(work_pending(&transfer_task)){ //Si hay trabajo pendiente esperar a que finalice
			flush_work(&transfer_task);
		}

		insert_cbuffer_t(cbuffer, numAleatorio);
		printk(KERN_INFO "Numero generado aleatoriamente: %i\n", numAleatorio);
		
		if(((size_cbuffer_t(cbuffer)/MAX_ITEMS_CBUF) * 100) >= emergency_treshold){
			cpuActual = smp_processor_id();
			schedule_work_on(~cpuActual, &transfer_task);
		}
	spin_unlock_irqrestore(&sp, flags);
  
    /* Re-activate the timer one second from now */
	mod_timer(&(my_timer), jiffies + msecs_to_jiffies(timer_period_ms)); //jiffies + HZ = 1s
	
}

//WORK///////////////////////////////////////////////////////////////////////////////
static void copy_items_into_list(struct work_struct *work){
	//TAREA DIFERIDA
	
	unsigned int numero;
	int numElementosBuffer = 0;
	unsigned long flags;
	int numElemeMovidos;

	spin_lock_irqsave(&sp, flags);
		numElementosBuffer = size_cbuffer_t(cbuffer);
		numElemeMovidos = numElementosBuffer;
	spin_unlock_irqrestore(&sp, flags);

	//BAJAR SEMAFORO
	if(!down_interruptible(&mtx)){
		while(numElementosBuffer > 0){
			spin_lock_irqsave(&sp, flags);
				numero = remove_cbuffer_t(cbuffer);
				numElementosBuffer = size_cbuffer_t(cbuffer);
			spin_unlock_irqrestore(&sp, flags);
		
			list_item_t *nodo = vmalloc(sizeof(list_item_t));
			nodo->data = numero;
			list_add_tail(&nodo->links, &mylist);
		}

		printk(KERN_INFO "%i elementos movidos del buffer circular a la lista\n", numElemeMovidos);

		//DESPERTAR AL PROGRAMA DE USUARIO QUE ESTE ESPERANDO A QUE HAYA ELEMENTOS EN LA LISTA
		if(procesosWaitingRead > 0){
			up(&mtx2);
			procesosWaitingRead--;
		}	
	}

	//SUBIR SEMAFORO
	up(&mtx);
}

//MODCONFIG//////////////////////////////////////////////////////////////////////////
static ssize_t modconfig_proc_read(struct file *fd, char __user *buf, size_t len, loff_t *off){
	char *dest;
	char *kbuff;

	kbuff = (char *)vmalloc(len);
	
	//Como queremos que no se este continuamente leyendo,  no comentamos la siguiente condicion

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
	//REQUISITOS
	// - Dormir si no hay elementos que leer.

	char *dest;
	char *kbuff;

	kbuff = (char *)vmalloc(len);
	dest = kbuff;
	
	list_item_t *item = NULL;
	struct list_head  *cur_node = NULL;
	struct list_head *aux = NULL;

	//lock candado de acceder a la lista
	if(down_interruptible(&mtx)){
		vfree(kbuff);
		return -EINTR;
	}

	while(list_empty(&mylist)){
		printk(KERN_INFO "El consumidor se duerme. No hay nada que leer\n");

		//cond_wait(cons, mtx);
		procesosWaitingRead++;
		up(&mtx);

		if(down_interruptible(&mtx2)){
			down(&mtx);
			procesosWaitingRead--;
			up(&mtx);
			vfree(kbuff);
			return -EINTR;
		}

		if(down_interruptible(&mtx)){
			vfree(kbuff);
			return -EINTR;
		}
	}

	list_for_each_safe(cur_node, aux, &mylist){
		item = list_entry(cur_node, list_item_t, links);
		dest += sprintf(dest, "%i\n", item->data);
		list_del(cur_node);
		vfree(item);
	}


	//unlock(mtx);
	up(&mtx);

	if (copy_to_user(buf, &kbuff[0], dest - kbuff)){
		vfree(kbuff);
		return -EINVAL;
	}

	(*off) += (dest-kbuff);

	return (dest-kbuff);
}

static int modtimer_proc_open(struct inode *node, struct file *fd){
	//REQUISITOS
	// - Incrementar el contador de referencias cuando se haga un open
	// - Si el contador de referencias es mayor que uno, no permitir NINGUNA operacion

	if((int)module_refcount(THIS_MODULE) > 1){
		printk(KERN_INFO "El modulo ya esta en uso. No entran mas");
		return 0;
	} 

	//incremento cont de referencias
	try_module_get(THIS_MODULE);

	//EMPIEZA LA CUENTA///////////////////////////////////////////////////////////////
	/* Activate the timer for the first time */
    add_timer(&my_timer);

	return 0;
}

static int modtimer_proc_release(struct inode *node, struct file *fd){
	//desactivar temporizador
	del_timer_sync(&my_timer);

	//esperar a la finalizacion de TODOS los trabajos
	flush_scheduled_work();

	//vaciado de buffer
	clear_cbuffer_t(cbuffer);

	//vaciar la lista enlazada
	list_item_t *item = NULL;
	struct list_head  *cur_node = NULL;
	struct list_head *aux = NULL;

	list_for_each_safe(cur_node, aux, &mylist){
		item = list_entry(cur_node, list_item_t, links);
		list_del(cur_node);
		vfree(item);
	}

	//derecemento del contador de referencias
	module_put(THIS_MODULE);

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
    //INICIALIZAR VARIABLES DE CONFIGURACIÓN//////////////////////////////////////////
	timer_period_ms = 500;
	emergency_treshold = 80;
	max_random = 300;

    //TEMPORIZADOR///////////////////////////////////////////////////////////////////
    /* Create timer */
    init_timer(&my_timer);
    /* Initialize field */
    my_timer.data = 0;
    my_timer.function = fire_timer;
    //my_timer.expires = jiffies + HZ;  /* Activate it one second from now */
    my_timer.expires = jiffies + msecs_to_jiffies(timer_period_ms);

    //BUFFER CIRCULAR/////////////////////////////////////////////////////////////////
	cbuffer = create_cbuffer_t(MAX_ITEMS_CBUF);
	if (!cbuffer) {
	   	return -ENOMEM;
	}
	  
	//SEMAFOROS///////////////////////////////////////////////////////////////////////
	sema_init(&mtx, 1);  //candado que comienza libre para su uso
	sema_init(&mtx2, 0); //para variables condicionales

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

	//Initialize work structure (with function)///////////////////////////////////////
	INIT_WORK(&transfer_task, copy_items_into_list);
      
	printk(KERN_INFO "ModTimer: Cargado el Modulo con total exito.\n");
	printk(KERN_INFO "ModConfig: Cargado el Modulo con total exito.\n");

	//INICIALIZAMOS LA LISTA ENLAZADA/////////////////////////////////////////////////
	INIT_LIST_HEAD(&mylist); /* Inicializamos la lista */
	  
	//EMPIEZA LA CUENTA///////////////////////////////////////////////////////////////
	/* Activate the timer for the first time */
    //add_timer(&my_timer);
    //se realiza en el open

    return 0;
}


void cleanup_modtimer_module( void ){
	
	/* Wait until completion of the timer function (if it's currently running) and delete timer */
	del_timer_sync(&my_timer);

	//CONTROLAR QUE CUANDO HAYA UN PROCESO QUE ESTË ESCRIBIENDO NO SE PUEDA DESCARGAR
	//esperar a la finalizacion de TODOS los trabajos
	flush_scheduled_work();

	remove_proc_entry("modtimer", NULL);
	remove_proc_entry("modconfig", NULL);
	destroy_cbuffer_t(cbuffer);
	printk(KERN_INFO "ModTimer: Modulo descargado con total exito.\n");
	printk(KERN_INFO "ModConfig: Modulo descargado con total exito.\n");

	if(!list_empty(&mylist)){
		printk(KERN_INFO "modlist: lista no vacia. Se borra la lista\n");

		list_item_t *item = NULL;
		struct list_head  *cur_node = NULL;
		struct list_head *aux = NULL;

		list_for_each_safe(cur_node, aux, &mylist){
			item = list_entry(cur_node, list_item_t, links);
			list_del(cur_node);
			vfree(item);
		}	
	}
}

module_init( init_modtimer_module );
module_exit( cleanup_modtimer_module );
