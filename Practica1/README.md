# Practica 1
Módulos y uso de listas dobles enlazadas

#Comandos de instalación
1.Realizar make en el directorio<br>
2.sudo insmod modlist.ko<br>
3.Interactuar con los distintos comandos implementados.<br>
4.Descargar módulo: sudo rmmod modlist<br>
#Comandos de uso
1.add: inserta x al final de la lista. <br>
2.remove: borra todas las ocurrencias de x en la lista.<br>
3.push: inserta x al principio de la lista.<br>
4.pop: elimina el primer elemento de la lista.<br>
5.sort: ordena todos los elementos x descendentemente. <br>
6.cleanup: elimina todos los elementos de la lista. <br>
#Parte 0
1.echo add numero entero > /proc/modlist<br>
2.echo remove numero entero > /proc/modlist<br>
3.echo cleanup > /proc/modlist

#Opcional 1 - Lista de cadenas de caracteres // nmros.enteros
0.make EXTRA_CFLAGS=-DPARTE_OPCIONAL // Para lista de cadenas de caracteres<br>
0.make // Para lista de números enteros<br>
1.echo add cadena de caracteres//numero entero > /proc/modlist<br>
2.echo remove cadena de caracteres//numero entero > /proc/modlist<br>
3.echo cleanup > /proc/modlist

#Opcional 2 - Lista de cadenas de caracteres // nmros.enteros
0.make EXTRA_CFLAGS=-DPARTE_OPCIONAL // Para lista de cadenas de caracteres<br>
0.make // Para lista de números enteros<br>
1.echo add cadena de caracteres//numero entero > /proc/modlist<br>
2.echo remove cadena de caracteres//numero entero > /proc/modlist<br>
3.echo push cadena de caracteres//numero entero > /proc/modlist<br>
4.echo pop > /proc/modlist<br>
5.echo sort > /proc/modlist<br>
6.echo cleanup > /proc/modlist
