obj-m += rsa_mod.o 
rsa_mod-objs := rsa.o rsa_common.o

all:
	KCPPFLAGS="-DKERNEL" make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

