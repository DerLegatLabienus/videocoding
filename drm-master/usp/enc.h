#ifndef __ENC_H__
#define __ENC_H__

#define EC_SIZE (4096*2)

typedef struct _enc_context {
    int ppos, cpos;
    int pdata[EC_SIZE], cdata[EC_SIZE];
    int bits;
} enc_context;

void init_enc_context(enc_context* ec);
void flip_enc_context(enc_context* ec);
int get_cbit(enc_context* ec);
void put_pbit(int bit, enc_context* ec);
void enc_update(int* val, enc_context* ec);

#endif
