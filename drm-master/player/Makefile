CC = gcc
IDIRS = -I ../tpvm
LFLAGS = -lSDL
CFLAGS = -g3 -DFETCHER -DTO_KERNEL

PLAYER = player 
FETCHER = fetcher
DECRYPTOR = decryptor

all: $(PLAYER) $(FETCHER) 

$(PLAYER): player.c common.c common.h ../tpvm/h264.h
	$(CC) $(CFLAGS) $(IDIRS) $^ -o $@ $(LFLAGS)

$(FETCHER): fetcher.c common.c common.h ../rsa/rsa.c ../rsa/rsa_common.c ../rsa/rsa_client.c
	$(CC) $(CFLAGS) $(IDIRS) $^ -o $@

$(DECRYPTOR): decryptor.c common.c common.h ../rsa/rsa.c ../rsa/rsa_common.c ../rsa/rsa_client.c 
	$(CC) $(CFLAGS) $(IDIRS) $^ -o $@ -I ../rsa

clobber clean:
	rm -f $(PLAYER) $(FETCHER) $(DECRYPTOR) drm_socket
