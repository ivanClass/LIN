#!/bin/bash

for i in `seq 1 5000`
do
	echo "Lista actual: " 
	cat /proc/modlist
	echo "remove $i" > /proc/modlist 
	sleep 0.5
done