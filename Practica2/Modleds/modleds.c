#include <linux/module.h> 
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0


struct tty_driver* kbd_driver= NULL;


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


	//Coger mensaje buf
	//Copiarlo a kbuff
	//

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

	//LLAMAR A setLeds con el numero de kbuff
	kbd_driver= get_kbd_driver_handler();
	set_leds(kbd_driver,ALL_LEDS_ON); 

	//FINAL COPIADO
	*off+=len;            /* Update the file pointer */
	vfree(kbuff);

	return len;
}

static int __init modleds_init(void)
{	
	proc_entry = proc_create( "modleds", 0666, NULL, &fops);

	if (proc_entry == NULL) {
		printk(KERN_INFO "Modleds: Can't create /proc entry\n");

		ret = -ENOMEM;
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

static const struct file_operations fops = {
	.write = mywrite
};

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modleds");
