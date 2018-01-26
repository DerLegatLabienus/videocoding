#ifndef _CRYPTO_RSA_H
#define _CRYPTO_RSA_H

/* 
 * struct rsa_op: multi-precision integer operand
 * @data: array that holds the number absolute value
 * @sign: 1 for negative, 0 for positive
 * @size: significant number limbs
 * @limbs: allocated limbs (sizeof data)
 */

#define CONFIG_RSA_AUX_OPERAND_SIZE 128
#define E_BYTE_SIZE 2

#ifndef KERNEL
#define u8   unsigned char
#define u32  unsigned int
#define u64  unsigned long
#endif

typedef struct rsa_ctx {
	rsa_op* e;
	rsa_op* d;
	rsa_op* n;
} rsa_ctx;

int	 rsa_difference(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux);
void rsa_generate_fi_n(rsa_op** res, rsa_op* p, rsa_op* q);
void rsa_generate_n (rsa_op** res, rsa_op* p, rsa_op* q);
//int	 rsa_op_alloc(rsa_op **, int);
void rsa_op_free(rsa_op *);
//int  rsa_op_init(rsa_op **, u8 *, u32, u32);
int  rsa_op_set(rsa_op **, u8 *, u32);
int	 rsa_op_copy(rsa_op **, rsa_op *);
//void rsa_op_print(rsa_op *, bool);
int  rsa_cipher(rsa_op **, rsa_op *, rsa_op *, rsa_op *);
void rsa_key_generation(rsa_ctx* ctx);
int	 rsa_op_random_prime_generator(rsa_op** n,u32 bit_size);
char rsa_op_compare(rsa_op *l, rsa_op *r);
void rsa_op_canonicalize(rsa_op *n);
int	 rsa_op_resize(rsa_op **n, int size);
int	 rsa_op_modular_inverse(rsa_op** res, rsa_op* e, rsa_op* fin, rsa_op** aux);
int	 rsa_product(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux);
void rsa_generate_fi_n(rsa_op** res, rsa_op* p, rsa_op* q);
int	 rsa_divide(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux);

u32  rsaLoad(void);
u32  rsaUnload(void);
void rsa_keygen(u8* buf);
void rsa_encrypt(u8* buf, u8* cipher_buf);
void rsa_gen_encrypted_key(u8* buf, u8* cipher_buf);
void rsa_decrypt(u8* buf, u8* msg_buf);

unsigned char checksum (unsigned char *ptr, int len);
#endif
