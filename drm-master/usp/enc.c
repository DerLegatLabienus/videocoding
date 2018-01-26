#include "enc.h"
#include <string.h>

void init_enc_context(enc_context* ec)
{
    int i;
    for (i = 0; i < EC_SIZE; i++) {
        ec->pdata[i] = 1;
        ec->cdata[i] = 0;
    }
    ec->ppos = ec->cpos = ec->bits = 0;
}


void flip_enc_context(enc_context* ec)
{
    int i;
    //printf("in flip: bits = %d\n", ec->bits);
    ec->bits = 0;

    // put encrypted plain data in cipher data
    memcpy(ec->cdata, ec->pdata, EC_SIZE*sizeof(unsigned int));
    // our poor man encryption - just flip the bits
    for (i = 0; i < ec->cpos; ++i)
        ec->cdata[i] ^= 1;

    // clear plain data
    memset(ec->pdata, 0, EC_SIZE*sizeof(unsigned int));

    ec->ppos = ec->cpos = 0;
}

int get_cbit(enc_context* ec)
{
    return ec->cdata[ec->cpos++];
}

void put_pbit(int bit, enc_context* ec)
{
    ec->pdata[ec->ppos++] = bit;
}

// put pbit and get cbit (in out parameter)
void enc_update(int* val, enc_context* ec)
{
    ec->bits++;
    if (ec->ppos > EC_SIZE || ec->cpos > EC_SIZE)
        return;
    put_pbit(*val > 0, ec);
    if (get_cbit(ec))
        *val *= -1;
}


