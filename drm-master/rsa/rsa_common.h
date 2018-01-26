#ifndef _CRYPTO_COMMON_H_
#define _CRYPTO_COMMON_H_

typedef struct rsa_op {
	unsigned int *data;
	int sign;
	int size;
	int limbs;
} rsa_op;

int rsa_op_alloc(rsa_op **n, int limbs);
int rsa_op_init(rsa_op **n, unsigned char *data, unsigned int size, unsigned int xtra);
void rsa_op_print(rsa_op *n, unsigned int how);
int rsa_op_serialize(rsa_op* op, unsigned char* buf,int startIndex);

int keygen(rsa_op** n, rsa_op** e, rsa_op** d);
int encrypt(rsa_op* n, rsa_op* e, unsigned char* m, unsigned char* c, int len);
int gen_encrypted_key(rsa_op* n, rsa_op* e, unsigned char* c);
int decrypt(rsa_op* n, rsa_op* d, unsigned char* c, unsigned char* m, int len);

int write_buffer(const char path[], unsigned char* buf, int len);
int read_buffer (const char path[], unsigned char* buf, int len);

#ifdef TPVM
#define KEY_LEN 256
extern unsigned char tpvm_key[KEY_LEN];
#endif

#endif
