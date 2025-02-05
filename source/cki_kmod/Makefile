obj-m += pkm.o
pkm-objs += pkm_main.o pkm_asm.o

all:
	make -C ../kernel M=$(PWD) modules

clean:
	make -C ../kernel M=$(PWD) clean

insmod:
	sudo insmod pkm.ko

rmmod:
	sudo rmmod pkm.ko
