#include "rc4.h"
#include <linux/module.h>
#include <linux/kernel.h>

static __inline void
swap_bytes(unsigned char *a, unsigned char *b)
{
	unsigned char temp;

	temp = *a;
	*a = *b;
	*b = temp;
}

/*
 * Initialize an RC4 state buffer using the supplied key,
 * which can have arbitrary length.
 */
void
rc4_init(rc4_state *state, unsigned char *key, int keylen)
{
	unsigned char j;
	int i;
                     
    char buff[16*3 + 1] = {0};
	printk(KERN_INFO "in rc4_init, keylen = %d", keylen);
	for (i = 0; i != 16; ++i) {
	    for (j = 0; j != 16; ++j) {
	        sprintf(buff + j*3, "%02x|", (unsigned char)key[i*16+j]);
        }
        printk(KERN_INFO "%s\n", buff);
    }

	/* Initialize state with identity permutation */
	for (i = 0; i < 256; i++)
		state->perm[i] = (unsigned char)i; 
	state->index1 = 0;
	state->index2 = 0;
  
	/* Randomize the permutation using key data */
	for (j = i = 0; i < 256; i++) {
		j += state->perm[i] + key[i % keylen]; 
		swap_bytes(&state->perm[i], &state->perm[j]);
	}
}

/*
 * Encrypt some data using the supplied RC4 state buffer.
 * The input and output buffers may be the same buffer.
 * Since RC4 is a stream cypher, this function is used
 * for both encryption and decryption.
 */
void
rc4_crypt(rc4_state *const state,
	unsigned char *inbuf, unsigned char *outbuf, int buflen)
{
	int i;
	unsigned char j;

	for (i = 0; i < buflen; i++) {

		/* Update modification indicies */
		state->index1++;
		state->index2 += state->perm[state->index1];

		/* Modify permutation */
		swap_bytes(&state->perm[state->index1],
		    &state->perm[state->index2]);

		/* Encrypt/decrypt next byte */
		j = state->perm[state->index1] + state->perm[state->index2];
		outbuf[i] = inbuf[i] ^ state->perm[j];
	}
}
