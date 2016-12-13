#!/bin/bash
#delay = ${1-0.5}

for i in `seq 1 8000`
do
	echo "add $i" > /proc/modlist 
	#sleep 0.5
done