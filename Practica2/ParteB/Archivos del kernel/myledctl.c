#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/module.h> 
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>
#include <linux/proc_fs.h>       /* For KDSETLED */
#include <linux/vt_kern.h>

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

SYSCALL_DEFINE1(ledctl, unsigned int, leds){
	//printk(KERN_DEBUG "Hello world!\n");
	int ret = 0;
	//CUERPO DE LA FUNCIÓN
	int a1 = leds & 0x4;
	int a2 = leds & 0x2;
	int a3 = leds & 0x1;

	a1 = a1 >> 1;
	a2 = a2 << 1;
	
	leds = a1 + a2 + a3;
	
	kbd_driver= get_kbd_driver_handler();

	ret = set_leds(kbd_driver, leds); 

	return ret;
}
