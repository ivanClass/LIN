#include <linux/module.h> 
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>
#include <linux/proc_fs.h>       /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Practica 2 LIN - FDI-UCM");
MODULE_AUTHOR("Ivan Aguilera Calle - Daniel Garcia Moreno");

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0
#define LED1_ON 0x2
#define LED2_ON 0x4
#define LED3_ON 0x1


struct tty_driver* kbd_driver= NULL;

static struct proc_dir_entry *proc_entry;

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

static ssize_t mywrite(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	char *kbuff; //CAMBIAR A NUMEROOOO?¿?¿? COPIAR DE SO PR4
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

	unsigned int i;
	sscanf(kbuff,"%x", &i);

	/*
	OPCION 1 !!!!!!!
	int mask = 0;
    if((i & LED1_ON) == LED1_ON){
    	mask |= LED2_ON;
    }
    if((i & LED2_ON) == LED2_ON){
    	mask |= LED1_ON;
    }
    if((i & LED3_ON) == LED3_ON){
    	mask |= LED3_ON;
    }
    set_leds(kbd_driver, mask); 

    */

	/*  a1 contiene el valor del bit de la izquieda del todo
		a2 contiene el valor del bit del centro
		a3 contiene el valor del bit de la derecha del todo

		Hacemos sus respectivas AND para conocer dichos valores

		Como el bit del centro corresponde al led de la derecha y viceversa, 
		se realizan desplazamientos para hacer un intercambio de bits (a3 se
		manetiene en su posición y por ello no se desplaza) y se suman
		a1 a2 y a3 para conocer el estado final
	*/
	
	int a1 = i & 0x4;
	int a2 = i & 0x2;
	int a3 = i & 0x1;

	a1 = a1 >> 1;
	a2 = a2 << 1;
	
	i = a1 + a2 + a3;
	
	kbd_driver= get_kbd_driver_handler();

	set_leds(kbd_driver, i); 


	*off+=len;            /* Update the file pointer */
	vfree(kbuff);

	return len;
}

static const struct file_operations fops = {
	.write = mywrite
};

static int __init modleds_init(void)
{	
	proc_entry = proc_create( "modleds", 0666, NULL, &fops);

	if (proc_entry == NULL) {
		printk(KERN_INFO "Modleds: Can't create /proc entry\n");

		return -ENOMEM;
	}
	else {
		printk(KERN_INFO "Modleds: Module loaded\n");
	}

	kbd_driver= get_kbd_driver_handler();
	set_leds(kbd_driver,ALL_LEDS_ON); 

	return 0;
}

static void __exit modleds_exit(void){
    set_leds(kbd_driver,ALL_LEDS_OFF); 
}

module_init(modleds_init);
module_exit(modleds_exit);