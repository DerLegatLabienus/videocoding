#ifndef _SYS_CRYPTO_RC4_RC4_H_
#define _SYS_CRYPTO_RC4_RC4_H_

typedef struct _rc4_state {
	unsigned char	perm[256];
	unsigned char	index1;
	unsigned char	index2;
} rc4_state;

extern void rc4_init(rc4_state *state, unsigned char *key, int keylen);
extern void rc4_crypt(rc4_state *state, unsigned char *inbuf, unsigned char *outbuf, int buflen);

#endif

