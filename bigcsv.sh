#! /bin/bash


for i in `seq 100000 -1 1`; do
	c=$(( (i / (256 * 256)) % 256 ))
	b=$(( (i / 256) %  256 ))
	a=$(( i % 256 ))
	echo "10.$c.$b.$a;32;TEST_$i"

done
