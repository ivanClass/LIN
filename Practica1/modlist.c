#include <linux/module.h>	/* Requerido por todos los módulos */
#include <linux/kernel.h>	/* Definición de KERN_INFO */
#include <linux/list.h> /* Lista doblemente enlazada */
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>

MODULE_LICENSE("GPL"); 	/*  Licencia del modulo */
MODULE_DESCRIPTION("Practica 1 LIN - FDI-UCM");
MODULE_AUTHOR("Ivan Aguilera Calle - Daniel Garcia Moreno");

static struct proc_dir_entry *proc_entry;

struct list_head mylist; /* Lista enlazada */

typedef struct {
	int data;
	struct list_head links;
} list_item_t;



static ssize_t myread(struct file *filp, char __user *buf, size_t len, loff_t *off) {
	char *dest;
	char *kbuff;

	kbuff = (char *)vmalloc(len);
	dest = kbuff;	

	if ((*off) > 0){ /* Tell the application that there is nothing left to read */
	  vfree(kbuff);
	  return 0;
	}

	list_item_t *item = NULL;
	struct list_head  *cur_node = NULL;

	list_for_each(cur_node, &mylist){
		item = list_entry(cur_node, list_item_t, links);
		dest += sprintf(dest, "%i\n", item->data);
	}

	/* Transfer data from the kernel to userspace */  
	if (copy_to_user(buf, &kbuff[0], dest - kbuff)){
		vfree(kbuff);
		return -EINVAL;
	}

	(*off) += (dest-kbuff);  /* Update the file pointer */
	
	return dest-kbuff; 
}

static ssize_t mywrite(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
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


	int num;

	if(sscanf(kbuff, "add %i", &num) == 1) {
		//printk(KERN_INFO "Modlist kbuff num:%d", num);

		//insertar final lista lista_add_tail(nodo,lista)
		list_item_t *nodo = vmalloc(sizeof(list_item_t));
		nodo->data = num;
		list_add_tail(&nodo->links, &mylist);
		
	}
	else if (sscanf(kbuff, "remove %i", &num) == 1){
		list_item_t *item = NULL;
		struct list_head  *cur_node = NULL;
		struct list_head *aux = NULL;

		list_for_each_safe(cur_node, aux, &mylist){
			item = list_entry(cur_node, list_item_t, links);
			if(item->data == num){
				list_del(cur_node);
				vfree(item);
			}
		}		
	}
	else if(strncmp(kbuff, "cleanup", 7) == 0){
		list_item_t *item = NULL;
		struct list_head  *cur_node = NULL;
		struct list_head *aux = NULL;

		list_for_each_safe(cur_node, aux, &mylist){
			item = list_entry(cur_node, list_item_t, links);
			list_del(cur_node);
			vfree(item);
		}		
	}


	*off+=len;            /* Update the file pointer */
	vfree(kbuff);

	return len;
}

static const struct file_operations fops = {
	.read = myread,
	.write = mywrite,
};

//COMANDOS
//insmod....
//rmmod
//dmesg | tail
//lsmod
/* Función que se invoca cuando se carga el módulo en el kernel */
int init_module(void)
{
	int ret = 0;

	proc_entry = proc_create( "modlist", 0666, NULL, &fops);
	if (proc_entry == NULL) {
	ret = -ENOMEM;

	//vfree(clipboard);
	printk(KERN_INFO "Modlist: Can't create /proc entry\n");
	} else {
		printk(KERN_INFO "Modlist: Module loaded\n");
		INIT_LIST_HEAD(&mylist); /* Inicializamos la lista */
	}

	/* Devolver 0 para indicar una carga correcta del módulo */
	return ret;
}

/* Función que se invoca cuando se descarga el módulo del kernel */
void cleanup_module(void)
{
	/* Liberar memoria si lista no está vacía */

	remove_proc_entry("modlist", NULL);
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

	printk(KERN_INFO "modlist: Module unloaded.\n");
}