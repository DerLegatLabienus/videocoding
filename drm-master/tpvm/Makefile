obj-m += tpvm.o

tpvm-objs :=  module.o module_proc.o module_efd.o common.o input.o queue.o in_queue.o coretrans.o nal.o cavlc.o \
	params.o slicehdr.o slice.o mbmodes.o residual.o block.o \
	mode_pred.o intra_pred.o mocomp.o main.o perf.o rc4.o protocol.o ../rsa/rsa.o ../rsa/rsa_common.o

all:
	KCPPFLAGS="-DKERNEL -DTPVM" make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
