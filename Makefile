obj-m = rb438_drv.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc assignment3.c -o assignment3 -lpthread -Wall

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

assignment:
	gcc assignment3.c -o assignment3 -lpthread -Wall