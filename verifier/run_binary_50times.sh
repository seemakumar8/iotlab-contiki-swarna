#!/bin/sh

END=20

for i in $(seq 1 $END); 
	do echo "Trial:" $i; 

	./server 40
	sleep 30
done
