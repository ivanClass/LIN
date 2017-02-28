#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>
#include <linux/proc_fs.h>       /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/semaphore.h>

MODULE_LICENSE("GPL");

/* Get a minor range for your devices from the usb maintainer */
#define USB_BLINK_MINOR_BASE	0 

#define NR_LEDS 8
#define NR_BYTES_BLINK_MSG 6
#define NR_SAMPLE_COLORS 4

#define NUM_BITS 11
#define N 2048
#define PRIMER_BIT_BLINK 3

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0

/* Structure to hold all of our device specific stuff */
struct usb_blink {
	struct usb_device		*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct kref				 kref;
};

#define to_blink_dev(d) container_of(d, struct usb_blink, kref)

static struct usb_driver blink_driver;

//VARIABLES DE CONFIGURACION
static unsigned int color;
static int timer_period;
static int max_val;

static int usarColor;

//static int contadorBinario;

typedef struct{
	int formatoBinario[NUM_BITS];
	int formatoDecimal;
}cntBin_t;

static cntBin_t contadorBinario;

static unsigned int coloresManueales[NR_LEDS];   //cuando la variable de configuracion COLOR sea -1, se atenderá a este array.
										//En el momento en que se configure la variable COLOR, se ignorará este array.

//TECLADO
struct tty_driver* kbd_driver = NULL;

/* Workqueue descriptor */
static struct workqueue_struct *my_wq;  

//WRAPPER PARA PODER PASAR file COMO PARÁMETRO Y PODER USAR messagess DE BLINKSTICK
typedef struct {
	struct work_struct my_work;
	struct usb_blink *dev;
} my_work_t;

/* Work descriptor */
my_work_t *transfer_task;

/* Structure that describes the kernel timer */
struct timer_list my_timer;


//SPINLOCK
DEFINE_SPINLOCK(sp);

//SEMAFOROS
struct semaphore mtx;
struct semaphore sem_ms;

int ms_writing = 0;
int ms_waiting = 0;

//VARIABLES AUXILIARES
static void resetearContador(void);
static int sumarFormatoBinario(void);
static void enviarMensaje(const int numBinario[NUM_BITS], int reset, int tecladoYblistick, struct usb_blink *devv);

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "modleds: loading\n");
   printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

//###############################################################################################################
static void fire_timer(unsigned long data)
{
	int cpuActual;
	//unsigned long flags;
	int bit_blink_modificado = 0;
	int i;
	int a1, a2, a3;


	printk(KERN_INFO "Temporizador!!!!!!!!!!\n");

	//spin_lock_irqsave(&sp, flags);
		
		
		printk(KERN_INFO "Temporizador!SP!!!!!!!!!\n");
		
		i = contadorBinario.formatoBinario[10] * 10000000000 + 
		contadorBinario.formatoBinario[9] * 1000000000 + contadorBinario.formatoBinario[8] * 100000000 +
		contadorBinario.formatoBinario[7] * 10000000 +contadorBinario.formatoBinario[6] * 1000000 +
		contadorBinario.formatoBinario[5] * 100000 +contadorBinario.formatoBinario[4] * 10000 +
		contadorBinario.formatoBinario[3] * 1000 +contadorBinario.formatoBinario[2] * 100 +
		contadorBinario.formatoBinario[1] * 10 +contadorBinario.formatoBinario[0] * 1;

		printk(KERN_INFO "Form.decimal %i\n", contadorBinario.formatoDecimal);
		printk(KERN_INFO "Form.binario %i\n", i);
		if(work_pending((struct work_struct *)transfer_task)){ //Si hay trabajo pendiente esperar a que finalice
			flush_work((struct work_struct *)transfer_task);
		}

		if(contadorBinario.formatoDecimal < (max_val - 1)){
			bit_blink_modificado = sumarFormatoBinario();

			printk(KERN_INFO "BIT_MODIFICAO %i\n", bit_blink_modificado);

			if(bit_blink_modificado){
				cpuActual = smp_processor_id();

				if(cpuActual){
					queue_work_on(0, my_wq, (struct work_struct *)transfer_task);
				}
				else{
					queue_work_on(1, my_wq, (struct work_struct *)transfer_task);
				}
			}
		}
		else{
			resetearContador();

			cpuActual = smp_processor_id();

			if(cpuActual){
				queue_work_on(0, my_wq, (struct work_struct *)transfer_task);
			}
				
			else{
				queue_work_on(1, my_wq, (struct work_struct *)transfer_task);
			}
		}

		i = (((4) * (contadorBinario.formatoBinario[2])) + ((2) * contadorBinario.formatoBinario[1]) + (contadorBinario.formatoBinario[0]));

		a1 = i & 0x4;
		a2 = i & 0x2;
		a3 = i & 0x1;

		a1 = a1 >> 1;
		a2 = a2 << 1;
				
		i = a1 + a2 + a3;
		
		kbd_driver= get_kbd_driver_handler();

		set_leds(kbd_driver, i);

		kbd_driver= get_kbd_driver_handler();

		set_leds(kbd_driver, i);
	
	//spin_unlock_irqrestore(&sp, flags);

    /* Re-activate the timer one second from now */
	mod_timer(&(my_timer), jiffies + msecs_to_jiffies(timer_period)); //jiffies + HZ = 1s
	
}

//WORK///////////////////////////////////////////////////////////////////////////////
static void cambiar_estado_leds(struct work_struct *work){
	//TAREA DIFERIDA
	
	my_work_t *my_work = (my_work_t *)work;

	enviarMensaje(contadorBinario.formatoBinario, 0, 0, my_work->dev);
}

/* 
 * Free up the usb_blink structure and
 * decrement the usage count associated with the usb device 
 */
static void blink_delete(struct kref *kref)
{
	struct usb_blink *dev = to_blink_dev(kref);

	usb_put_dev(dev->udev);
	vfree(dev);
}

/* Called when a user program invokes the open() system call on the device */
static int blink_open(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);
	
	/* Obtain reference to USB interface from minor number */
	interface = usb_find_interface(&blink_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n",
			__func__, subminor);
		return -ENODEV;
	}

	/* Obtain driver data associated with the USB interface */
	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	/* increment our usage count for the device */
	kref_get(&dev->kref);

	/* save our object in the file's private structure */
	file->private_data = dev;
	transfer_task->dev = file->private_data;

	return retval;
}

/* Called when a user program invokes the close() system call on the device */
static int blink_release(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* decrement the count on our device */
	kref_put(&dev->kref, blink_delete);
	return 0;
}

static ssize_t blink_read(struct file *file, const char *user_buffer,
			  size_t len, loff_t *off)
{
	char *dest;
	char *kbuff;

	kbuff = (char *)vmalloc(len);
	
	//Como queremos que no se este continuamente leyendo,  no se comenta la siguiente condicion
	if ((*off) > 0){ /* Tell the application that there is nothing left to read */
		vfree(kbuff);
		return 0;
	}

	dest = kbuff;

	dest += sprintf(kbuff, "color = %x\ntimer_period = %i\nmax_val = %i\n", color, timer_period, max_val);

	/* Transfer data from the kernel to userspace */  
	if (copy_to_user(user_buffer, &kbuff[0], dest - kbuff)){
		vfree(kbuff);
		return -EINVAL;
	}

	(*off) += (dest-kbuff);  /* Update the file pointer */
	
	return (dest-kbuff);
}

/* Called when a user program invokes the write() system call on the device */
static ssize_t blink_write(struct file *file, const char *user_buffer,
			  size_t len, loff_t *off)
{
	unsigned int colorr;
	int nled = 0;
	char *kbuff;
	const char *delimitador = ",";
	char *token;
	int num;

	struct usb_blink *dev=file->private_data;
	int retval = 0;
	int i = 0;
	unsigned char messages[NR_LEDS][NR_BYTES_BLINK_MSG];
	//unsigned long flags;

	kbuff = (char *)vmalloc(len);

	//if ((*off) > 0){ 
	//	vfree(kbuff);/* The application can write in this entry just once !! */
	//	return 0;
	//}

	/* Transfer data from user to kernel space */
	if (copy_from_user(&kbuff[0], user_buffer, len )){
		vfree(kbuff);
		return -EFAULT;
	} 

	kbuff[len] = '\0';


	if(strncmp(kbuff, "start", 5) == 0){
		if((int)module_refcount(THIS_MODULE) >= 2){
			printk(KERN_INFO "START: El contador ya esta en funcionamiento. Parelo antes!");
			return -EBUSY;
		}
		else{
			printk(KERN_INFO "Opcion START seleccionado\n");
			//incremento cont de referencias
			try_module_get(THIS_MODULE);
			//Modo contador binario
			//transfer_task->file = file;
			enviarMensaje(NULL, 1, 1, file->private_data);
			add_timer(&my_timer);
		}
	}
	else if(strncmp(kbuff, "pause", 5) == 0){
		if((int)module_refcount(THIS_MODULE) == 1){
			printk(KERN_INFO "PAUSE: No hay ningún contador corriendo en estos instantes!");
			return -EBUSY;
		}
		else{
			//desactivar temporizador
			printk(KERN_INFO "Opcion PAUSE seleccionado\n");
			del_timer_sync(&my_timer);
		}
	}
	else if(strncmp(kbuff, "resume", 6) == 0){
		if((int)module_refcount(THIS_MODULE) == 1){
			printk(KERN_INFO "RESUME: No hay ningún contador corriendo en estos instantes!");
			return -EBUSY;
		}
		else{
			//Modo contador binario
			printk(KERN_INFO "Opcion RESUME seleccionado\n");
			mod_timer(&(my_timer), jiffies + msecs_to_jiffies(timer_period));
		}
	}
	else if(strncmp(kbuff, "stop", 4) == 0){
		if((int)module_refcount(THIS_MODULE) == 1){
			printk(KERN_INFO "STOP: No hay ningún contador corriendo en estos instantes!");
			return -EBUSY;
		}
		else{
			//desactivar temporizador
			printk(KERN_INFO "Opcion STOP seleccionado\n");
			del_timer_sync(&my_timer);

			//spin_lock_irqsave(&sp, flags);
				resetearContador();
			//spin_unlock_irqrestore(&sp, flags);

			enviarMensaje(NULL, 1, 1, file->private_data);
		
			//derecemento del contador de referencias
			module_put(THIS_MODULE); 
		}
	}
	else if(sscanf(kbuff, "value %i", &num) == 1){
		//Modo binario
		printk(KERN_INFO "CONTADOR DE REFERENCIAS: %i", (int)module_refcount(THIS_MODULE));
		if((int)module_refcount(THIS_MODULE) >= 2){
			printk(KERN_INFO "VALUE: El contador ya esta en funcionamiento. Parelo antes!");
			return -EBUSY;
		}
		else{
			printk(KERN_INFO "Opcion VALUE seleccionado\n");

			int numBinario[NUM_BITS];

			int aux = num;

			for(i = 0; i < NUM_BITS; i++){
				if(aux != 0){
					numBinario[i] = aux % 2;
					aux = aux / 2;
				}
				else{
					numBinario[i] = 0;
				}
			}

			enviarMensaje(numBinario, 0, 1, file->private_data);

		}

	}
	else if(sscanf(kbuff, "timer_period %d", &timer_period) == 1) {
		printk(KERN_INFO "Nuevo valor de timer_period: %d\n", timer_period);
		
	}
	else if (sscanf(kbuff, "color %x", &colorr) == 1){
		int j;
		for(j = 0; j < NR_LEDS; j++)
			coloresManueales[j] = colorr;

		usarColor = 1;
		color = colorr;
		printk(KERN_INFO "Nuevo valor de color: %x\n", colorr);	
	}
	else if (sscanf(kbuff, "max_val %d", &max_val) == 1){
		printk(KERN_INFO "Nuevo valor de max_val %d\n", max_val);	
	}
	else{
		if((int)module_refcount(THIS_MODULE) >= 2){
			printk(KERN_INFO "X:X,Y:Y, El contador ya esta en funcionamiento. Parelo antes!");
			return -EBUSY;
		}
		else{
			printk(KERN_INFO "Valores individuales LED modificados\n");
			token = strsep(&kbuff, delimitador);

			if(len > 1){
				while(token != NULL){
					if(sscanf(token, "%d:%x", &nled, &colorr) == 2){
						coloresManueales[nled] = colorr;			
					}
					else{
						(*off) += len;
						vfree(kbuff);
						return len;
					}

					token = strsep(&kbuff, delimitador);
				}

				usarColor = 0;	//La variable "color" deja de ser utilizada hasta que reciba un nuevo valor por terminal		
			}			
		}
	}

	vfree(kbuff);
	(*off)+=len;
	return len;	
}


/*
 * Operations associated with the character device 
 * exposed by driver
 * 
 */
static const struct file_operations blink_fops = {
	.owner =	THIS_MODULE,
	.write =	blink_write,	 	/* write() operation on the file */
	.open =		blink_open,			/* open() operation on the file */
	.release =	blink_release, 		/* close() operation on the file */
	.read    =  blink_read,         /* read() operation on the file */
};

/* 
 * Return permissions and pattern enabling udev 
 * to create device file names under /dev
 * 
 * For each blinkstick connected device a character device file
 * named /dev/usb/blinkstick<N> will be created automatically  
 */
char* set_device_permissions(struct device *dev, umode_t *mode) 
{
	if (mode)
		(*mode)=0666; /* RW permissions */
 	return kasprintf(GFP_KERNEL, "usb/%s", dev_name(dev)); /* Return formatted string */
}


/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver blink_class = {
	.name =		"blinkstick%d",  /* Pattern used to create device files */	
	.devnode=	set_device_permissions,	
	.fops =		&blink_fops,
	.minor_base =	USB_BLINK_MINOR_BASE,
};

/*
 * Invoked when the USB core detects a new
 * blinkstick device connected to the system.
 */
static int blink_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_blink *dev;
	int retval = -ENOMEM;

	/*
 	 * Allocate memory for a usb_blink structure.
	 * This structure represents the device state.
	 * The driver assigns a separate structure to each blinkstick device
 	 *
	 */
	dev = vmalloc(sizeof(struct usb_blink));

	if (!dev) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error;
	}

	/* Initialize the various fields in the usb_blink structure */
	kref_init(&dev->kref);
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &blink_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev,
			"Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */	
	dev_info(&interface->dev,
		 "Blinkstick device now attached to blinkstick-%d",
		 interface->minor);
	return 0;

error:
	if (dev)
		/* this frees up allocated memory */
		kref_put(&dev->kref, blink_delete);
	return retval;
}

/*
 * Invoked when a blinkstick device is 
 * disconnected from the system.
 */
static void blink_disconnect(struct usb_interface *interface)
{
	struct usb_blink *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &blink_class);

	/* prevent more I/O from starting */
	dev->interface = NULL;

	/* decrement our usage count */
	kref_put(&dev->kref, blink_delete);

	dev_info(&interface->dev, "Blinkstick device #%d has been disconnected", minor);
}

/* Define these values to match your devices */
#define BLINKSTICK_VENDOR_ID	0X20A0
#define BLINKSTICK_PRODUCT_ID	0X41E5

/* table of devices that work with this driver */
static const struct usb_device_id blink_table[] = {
	{ USB_DEVICE(BLINKSTICK_VENDOR_ID,  BLINKSTICK_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, blink_table);

static struct usb_driver blink_driver = {
	.name =		"blinkstick",
	.probe =	blink_probe,
	.disconnect =	blink_disconnect,
	.id_table =	blink_table,
};

/* Module initialization */
int blinkdrv_module_init(void)
{
	int j = 0, ret = 0;

	//INICIALIZAR VARIABLES DE CONFIGURACIÓN//////////////////////////////////////////
	timer_period = 500;
	color = 0x101500;
	max_val = N;

	//INICIALIZAR COLA DE TRABAJAO
	//INIT_WORK(&transfer_task, cambiar_estado_leds);
	my_wq = create_workqueue("my_queue");
	if(my_wq){
		transfer_task = (my_work_t *)kmalloc(sizeof(my_work_t), GFP_KERNEL);

		if(transfer_task){
			/* Initialize work structure (with function) */
      		INIT_WORK( (struct work_struct *)transfer_task, cambiar_estado_leds);

      		/* Enqueue work */
      		//ret = queue_work(my_wq, (struct work_struct *)transfer_task);
		}
	}

	//TEMPORIZADOR///////////////////////////////////////////////////////////////////
    /* Create timer */
    init_timer(&my_timer);
    /* Initialize field */
    my_timer.data = 0;
    my_timer.function = fire_timer;
    //my_timer.expires = jiffies + HZ;  /* Activate it one second from now */
    my_timer.expires = jiffies + msecs_to_jiffies(timer_period);

	//INICIALIZAR CONTADOR BINARIO
	contadorBinario.formatoDecimal = 0;
	
	for(j = 0; j < NUM_BITS; j++)
		contadorBinario.formatoBinario[j] = 0;

	//ARRAY MANUAL DE COLORES
	for(j = 0; j < NR_LEDS; j++)
		coloresManueales[j] = color;

	usarColor = 1;

	//SEMAFOROS///////////////////////////////////////////////////////////////////////
	sema_init(&mtx, 1);  //candado que comienza libre para su uso
	sema_init(&sem_ms, 0); //para variables condicionales

	//DRIVER TECLADO
	kbd_driver = get_kbd_driver_handler();
	set_leds(kbd_driver, ALL_LEDS_OFF);

   return usb_register(&blink_driver);
}

/* Module cleanup function */
void blinkdrv_module_cleanup(void)
{
	
	/* Wait until completion of the timer function (if it's currently running) and delete timer */
	del_timer_sync(&my_timer);

	//esperar a la finalizacion de TODOS los trabajos
	flush_scheduled_work();

	set_leds(kbd_driver,ALL_LEDS_OFF);
	usb_deregister(&blink_driver);

	/* Destroy workqueue resources */
  	destroy_workqueue(my_wq);
	
}

module_init(blinkdrv_module_init);
module_exit(blinkdrv_module_cleanup);

//FUNCIONES AUXILIARES//////////////////////////////////////////////////////////////////////////////////////////////////

static int sumarFormatoBinario(void){

	int i = 0;
	int acarreo = 1;
	int bits_blistick_modificados = 0;
	unsigned long flags;

	spin_lock_irqsave(&sp, flags);
		while(acarreo && (i < NUM_BITS)){
			
			if(i == PRIMER_BIT_BLINK){
				bits_blistick_modificados = 1;
			}

			if(contadorBinario.formatoBinario[i] == 1){
				contadorBinario.formatoBinario[i] = 0;
			}
			else{
				contadorBinario.formatoBinario[i] = 1;
				acarreo = 0;
			}

			i++;
			
		}
		
		contadorBinario.formatoDecimal++;
	spin_unlock_irqrestore(&sp, flags);


	return bits_blistick_modificados; //Devuelve 1 si algunos de los bits del blinstick ha sido modificado. 0 en otro caso.
}

static void resetearContador(void){
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&sp, flags);
		contadorBinario.formatoDecimal = 0;

		for(i = 0; i < NUM_BITS; i++){
			contadorBinario.formatoBinario[i] = 0;
		}
	spin_unlock_irqrestore(&sp, flags);
}

static void enviarMensaje(const int numBinario[NUM_BITS], int reset, int tecladoYblistick, struct usb_blink *devv){
	struct usb_blink *dev = devv;
	int retval = 0;
	int i = 0;
	unsigned char messages[NR_LEDS][NR_BYTES_BLINK_MSG];

	/* zero fill*/
	memset(messages, 0, sizeof(messages[0][0]) * NR_BYTES_BLINK_MSG * NR_LEDS);

	//lock(mtx)
	if(down_interruptible(&mtx)){
		
	}
	else{
		while(ms_writing > 0){
			ms_waiting++;
			up(&mtx);

			if(down_interruptible(&sem_ms)){
				down(&mtx);
					ms_waiting--;
				up(&mtx);
			
			}
			else{
				if(down_interruptible(&mtx)){
				
				}
			}
		}

		for(i = 0; i < NR_LEDS; i++){
			messages[i][0] ='\x05';
			messages[i][1] = 0x00;
			messages[i][2] = i;


			if(reset){
				messages[i][3] = ((0x000000 >> 16) & 0xff); //ROJO
		 		messages[i][4] = ((0x000000 >> 8) & 0xff);  //VERDE
		 		messages[i][5] = (0x000000 & 0xff);
			}
			else{
				if(numBinario[i + 3] == 0){
					messages[i][3] = ((0x000000 >> 16) & 0xff); //ROJO
		 			messages[i][4] = ((0x000000 >> 8) & 0xff);  //VERDE
		 			messages[i][5] = (0x000000 & 0xff); 
				}
				else{
					if(usarColor != 0){
						messages[i][3] = ((color >> 16) & 0xff); //ROJO
			 			messages[i][4] = ((color >> 8) & 0xff);  //VERDE
			 			messages[i][5] = (color & 0xff);
					}
					else{
						messages[i][3] = ((coloresManueales[i] >> 16) & 0xff); //ROJO
			 			messages[i][4] = ((coloresManueales[i] >> 8) & 0xff);  //VERDE
			 			messages[i][5] = (coloresManueales[i] & 0xff);
					}
					
				}
			}
			

			retval = usb_control_msg(dev->udev,	
				 usb_sndctrlpipe(dev->udev,00),  //Specify endpoint #0 
				 USB_REQ_SET_CONFIGURATION, 
				 USB_DIR_OUT| USB_TYPE_CLASS | USB_RECIP_DEVICE,
				 0x5,	/* wValue */
				 0, 	/* wIndex=Endpoint # */
				 messages[i],	/* Pointer to the message */ 
				 NR_BYTES_BLINK_MSG, /* message's size in bytes */
				 0);		

			if (retval < 0){
				printk(KERN_ALERT "Executed with retval=%d\n",retval);
			}
		}

		if(tecladoYblistick){
			if(!reset){
				i = ( (4 * (numBinario[2])) + ( 2 * (numBinario[1])) + (numBinario[0]) );
				printk(KERN_INFO "VALUE dec: %i\n", i);
				printk(KERN_INFO "VALUE bin: %i\n", (numBinario[2] * 100) + (numBinario[1] * 10) + (numBinario[0] * 1));

				int a1 = i & 0x4;
				int a2 = i & 0x2;
				int a3 = i & 0x1;

				a1 = a1 >> 1;
				a2 = a2 << 1;
						
				i = a1 + a2 + a3;
				
				kbd_driver= get_kbd_driver_handler();

				set_leds(kbd_driver, i);	
			}
			else{
				kbd_driver= get_kbd_driver_handler();

				set_leds(kbd_driver, 0);
			}
			
		}

		if(ms_waiting > 0){
			up(&sem_ms);
			ms_writing--;
		}
	}


	//unlock(mtx);
	up(&mtx);
}