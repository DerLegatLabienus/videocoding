CFLAGS=-Wall -O0 -g3 -DBUILD_TESTS 
#-std=c99

INPUT_MODULE=in_queue.o
#INPUT_MODULE=in_socket.o

LISTENER_MODULE=listener_file.o
#LISTENER_MODULE=listener_socket.o

CC=gcc
LD=gcc

LDFLAGS=
LIBS= -lpthread -lSDL

OBJS= $(INPUT_MODULE) $(LISTENER_MODULE) \
	  common.o input.o coretrans.o nal.o cavlc.o \
	params.o slicehdr.o slice.o mbmodes.o residual.o block.o \
	mode_pred.o intra_pred.o mocomp.o \
	main.o perf.o enc.o rc4.o queue_lock_mutex.o 

all:	playh264

playh264:	libh264.a playh264.o
	$(LD) $(LDFLAGS) playh264.o libh264.a -o playh264 $(LIBS)


libh264:	libh264.a
libh264.a:	$(OBJS)
	ar r libh264.a $(OBJS)

tests:	$(OBJS)
	@rm -f .test*
	@touch .test.log
	@\
for module in $(OBJS) ; do \
  mod=$$(basename $$module .o) ;\
  echo "int main(int argc, char* argv[]) { return _test_$$mod(argc,argv); }" >.test.c ;\
  echo "Making $$mod-test ..." ;\
  $(CC) $(CFLAGS) -c .test.c -o .test.o 2>/dev/null && \
  $(LD) $(LDFLAGS) $(LIBS) $(OBJS) .test.o -o $$mod-test 2>/dev/null && \
  echo "  $$mod-test" >>.test.log ;\
done
	@echo "The following test programs have been successfully compiled:"
	@cat .test.log
	@rm -f .test*

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

clean:
	rm -f $(OBJS) libh264.a *-test playh264 playh264.o
