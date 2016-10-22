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

#ifdef PARTE_OPCIONAL
	typedef struct {
		char *data;
		struct list_head links;
	} list_item_t;
#else
	typedef struct {
		int data;
		struct list_head links;
	} list_item_t;
#endif	


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

		#ifdef PARTE_OPCIONAL
			dest += sprintf(dest, "%s", item->data);
		#else
			dest += sprintf(dest, "%i\n", item->data);
		#endif
		
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

	#ifdef PARTE_OPCIONAL
		char *cadena;

		if(strncmp(kbuff, "add ", 4) == 0 ) {

			cadena = kbuff + 4;
			printk(KERN_INFO "Modlist cadena:%s", cadena);


			list_item_t *nodo = vmalloc(sizeof(list_item_t));
			nodo->data = vmalloc(strlen(cadena) + 1);
			strcpy(nodo->data, cadena);
			list_add_tail(&nodo->links, &mylist);
			
		}
		else if (strncmp(kbuff, "remove ", 7) == 0 ){
			list_item_t *item = NULL;
			struct list_head  *cur_node = NULL;
			struct list_head *aux = NULL;

			cadena = kbuff + 7;
			list_for_each_safe(cur_node, aux, &mylist){
				item = list_entry(cur_node, list_item_t, links);
				if(strcmp(item->data, cadena) == 0){
					list_del(cur_node);
					vfree(item);
				}
			}		
		}
		else if(strncmp(kbuff, "push ", 5) == 0 ) {
			cadena = kbuff + 5;
			//insertar final lista lista_add_tail(nodo,lista)
			list_item_t *nodo = vmalloc(sizeof(list_item_t));
			nodo->data = vmalloc(strlen(cadena) + 1);
			strcpy(nodo->data, cadena);
			list_add(&nodo->links, &mylist);
		}
		else if(strncmp(kbuff, "pop", 3) == 0){
			list_item_t *item = NULL;
			struct list_head  *cur_node = NULL;
			struct list_head *aux = NULL;

			if(!list_empty(&mylist)){
				cur_node = (mylist).next;
				item = list_entry(cur_node, list_item_t, links);
				list_del(cur_node);
				vfree(item);
			}
					
		}
		else if(strncmp(kbuff, "sort", 4) == 0){
			list_item_t *item = NULL;
			list_item_t *item2 = NULL;
			struct list_head  *cur_node = NULL;
			struct list_head *aux = NULL;
			struct list_head  *cur_node2 = NULL;
			struct list_head *aux2 = NULL;

			char *miAux;
			int inter = 1;
			//FUNCIONABA EN UN PRINCIPIO PERO AL INCLUIR '\0' A LAS CADENAS
			/*
			list_for_each_safe(cur_node2, aux2, &mylist){
				inter = 0;
				list_for_each_safe(cur_node, aux, &mylist){
					item = list_entry(cur_node, list_item_t, links);
					item2 = list_entry(cur_node->next, list_item_t, links);
					if(strcmp(item->data, item2->data) < 0){

						printk(KERN_INFO "Somos iguales");
						
						
						miAux = vmalloc(strlen(item->data) + 1);
						strcpy(miAux, item->data);

						vfree(item->data);
						item->data = vmalloc(strlen(item2->data) + 1);
						strcpy(item->data, item2->data);

						vfree(item2->data);
						item2->data = vmalloc(strlen(miAux) + 1);
						strcpy(item2->data, miAux);

						vfree(miAux);
						inter = 1;
						
					}
				}
				if(inter == 0){
					aux2 = &mylist;
					printk(KERN_INFO "He parado\n");

				}
			}*/
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
	#else
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
		else if(sscanf(kbuff, "push %i", &num) == 1) {
			//printk(KERN_INFO "Modlist kbuff num:%d", num);

			//insertar final lista lista_add_tail(nodo,lista)
			list_item_t *nodo = vmalloc(sizeof(list_item_t));
			nodo->data = num;
			list_add(&nodo->links, &mylist);
			
		}
		else if(strncmp(kbuff, "pop", 3) == 0){
			printk(KERN_INFO "holaaaaaaa");

			list_item_t *item = NULL;
			struct list_head  *cur_node = NULL;
			struct list_head *aux = NULL;

			if(!list_empty(&mylist)){
				cur_node = (mylist).next;
				item = list_entry(cur_node, list_item_t, links);
				list_del(cur_node);
				vfree(item);
			}
					
		}
		else if(strncmp(kbuff, "sort", 4) == 0){
			list_item_t *item = NULL;
			list_item_t *item2 = NULL;
			struct list_head  *cur_node = NULL;
			struct list_head *aux = NULL;
			struct list_head  *cur_node2 = NULL;
			struct list_head *aux2 = NULL;

			int miAux;
			int inter = 1;

			list_for_each_safe(cur_node2, aux2, &mylist){
				inter = 0;
				list_for_each_safe(cur_node, aux, &mylist){

					item = list_entry(cur_node, list_item_t, links);
					item2 = list_entry(cur_node->next, list_item_t, links);
					if(item->data < item2->data){
						miAux = item->data;
						item->data = item2->data;
						item2->data = miAux;
						inter = 1;
					}
				}
				if(inter == 0){
					aux2 = &mylist;
					printk(KERN_INFO "He parado\n");

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

	#endif

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

			#ifdef PARTE_OPCIONAL
				vfree(item->data);
			#endif
				
			list_del(cur_node);
			vfree(item);
		}	
	}

	printk(KERN_INFO "modlist: Module unloaded.\n");
}