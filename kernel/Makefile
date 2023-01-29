K ?= /lib/modules/$(shell uname -r)/build

obj-m += tx433.o

modules clean:
	make -C $(K) M=$(PWD) $@

