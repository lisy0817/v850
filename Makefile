v850:v850.c
	arm-poky-linux-gnueabi-gcc -o v850 -lpthread v850.c

clean:
	rm -rf v850
