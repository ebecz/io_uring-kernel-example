obj-m += rw_iter.o

all: main module

main: main.c
	gcc -o main main.c -luring

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	 make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

