/*
 * Cryptographic API
 *
 * RSA cipher algorithm implementation
 * The code was originally developed by Tasos Parisinos of the Sciensis Advanced Technology Systems
 * and was later extended by Asaf Algawi of Tel Aviv Academic College 
 * Copyright (c) 2012 Tasos Parisinos <t.parisinos@sciensis.com> & Asaf Algawi <asaf.algawi@gmail.com>
 * 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 */
//#define DEBUG
//#define KERNEL

#include <linux/netlink.h>
#include <linux/socket.h>
#include "msgtypes.h"

#ifdef KERNEL
#include <linux/string.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/string.h>

#else
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define printk printf
#define KERN_INFO
#define EINVAL -1
#define min(x, y) ({                \
    typeof(x) _min1 = (x);          \
    typeof(y) _min2 = (y);          \
    (void) (&_min1 == &_min2);      \
    _min1 < _min2 ? _min1 : _min2; })
#define max(x, y)        ((x > y)? x: y)
#endif

#include "rsa_common.h"
#include "rsa.h"
//#define NETLINK_USER 31

#define AUX_OPERAND_COUNT	8
#define U32_MAX		0xFFFFFFFF
#define U32_MSB_SET	0x80000000
#define LEFTOP		(aux[0])
#define RIGHTOP		(aux[1])

static rsa_op* one;
static rsa_op* two;
static rsa_op* zero;

#ifdef KERNEL
struct sock *nl_sk = NULL;
#endif

unsigned char checksum (unsigned char *ptr, int len) {
    unsigned char chk = 0;
    while (len-- != 0)
        chk -= *ptr++;
    return chk;
}

void rsa_op_canonicalize(rsa_op *n)
{
	int i;
	u32 *buf = n->data;

	for (i = n->size - 1; i >= 0; i--)
		if (!buf[i] && n->size > 1)
			n->size--;
		else
			break;
}

#ifdef KERNEL

static void send_msg_done(struct nlmsghdr** nlh, u8* buf, int len) {
    int pid = (*nlh)->nlmsg_pid;
    struct sk_buff* out = nlmsg_new(len,0);
    *nlh = nlmsg_put(out,0,0,NLMSG_DONE,len,0);
    NETLINK_CB(out).dst_group = 0;
    memcpy(nlmsg_data(*nlh),buf,len);
    nlmsg_unicast(nl_sk, out, pid);
}

#endif

void rsa_keygen(u8* buf)
{
    rsa_ctx ctx = {0};
			
    int len1, len2;
    memset(buf, 0x0, 640);
    rsa_key_generation(&ctx);
    rsa_op_canonicalize(ctx.d);
    rsa_op_canonicalize(ctx.n);
    len1 = rsa_op_serialize(ctx.e, buf,     0);
    len2 = rsa_op_serialize(ctx.d, buf+128, 0);
    len2 = rsa_op_serialize(ctx.n, buf+384, 0);			
}

void rsa_encrypt(u8* buf, u8* cipher_buf)
{
    rsa_op *e, *n, *m, *c;
    memset(cipher_buf,0,256);			
    rsa_op_alloc(&e,1);
    rsa_op_set(&e,buf+1,4);
    rsa_op_alloc(&n,64);
    rsa_op_set(&n,buf+129,256);
    rsa_op_alloc(&m,64);
    rsa_op_set(&m,buf+768,256);
//    rsa_op_print(m, 0);

    rsa_op_alloc(&c,64);
    rsa_cipher(&c,m,e,n);
//    rsa_op_print(c, 0);
    rsa_op_canonicalize(c);
    //sending the cipher back
    rsa_op_serialize(c,cipher_buf,0);			
}

void rsa_decrypt(u8* buf, u8* msg_buf)
{
    rsa_op *d, *n, *m, *c;
    memset(msg_buf,0,256);	
    rsa_op_alloc(&d,64);
    rsa_op_set(&d,buf+1,256);
//    rsa_op_print(d, 0);
    rsa_op_alloc(&n,64);
    rsa_op_set(&n,buf+257,256);
//    rsa_op_print(n, 0);
    rsa_op_alloc(&c,64);
    rsa_op_set(&c,buf+768,256);
//    rsa_op_print(c, 0);
    rsa_op_alloc(&m,64);
    rsa_cipher(&m,c,d,n);
    rsa_op_canonicalize(m);
//    rsa_op_print(m, 0);
    //sending the msg back
    rsa_op_serialize(m,msg_buf,0);
}

void rsa_gen_encrypted_key(u8* buf, u8* cipher_buf)
{
    int i,j;
    char pretty_buf[16*3 + 1];

    rsa_op *e, *n, *m, *c;
#ifdef TPVM
    unsigned char* key = tpvm_key;
#else
    unsigned char key_buf[256] = {0};
    unsigned char* key = key_buf;
#endif
    memcpy(key, "daviddavid", strlen("daviddavid"));
    /*
#ifndef KERNEL
    int randptr = open("/dev/urandom", O_RDONLY);
    int r = read(randptr,key,256);
    printk("read %d chars from /dev/urandom into key\n", r);
    close(randptr);
#else
    get_random_bytes(key,256);
    printk("read %d chars using get_random_bytes, key[0] = %02x\n", 256, key[0]);
#endif
*/
    memset(cipher_buf,0,256);			
    rsa_op_alloc(&e,1);
    rsa_op_set(&e,buf+1,4);
    rsa_op_alloc(&n,64);
    rsa_op_set(&n,buf+129,256);
    rsa_op_alloc(&m,64);

    rsa_op_set(&m,key,256);

    rsa_op_alloc(&c,64);
    rsa_cipher(&c,m,e,n);
    rsa_op_canonicalize(c);
    rsa_op_serialize(c,cipher_buf,0);			
}

#ifdef KERNEL

static void netlink_dispatcher(struct sk_buff* skb) {
    struct nlmsghdr *nlh;
    u8 msgtype = '0';
    //int pid;
    //struct sk_buff* out;
    //
    

    nlh = (struct nlmsghdr*)skb->data;
    msgtype = ((u8*) NLMSG_DATA(nlh))[0];

    printk("message aviv");
    switch(msgtype)
    {
        case KEYGEN:
            {
                u8 buf[640];
                rsa_keygen(buf);
                send_msg_done(&nlh, buf, 640); 

                break;
            }
        case ENCRYPT:
            {
                u8 *buf = ((u8*) NLMSG_DATA(nlh));
                u8 cipher_buf[256];
                rsa_encrypt(buf, cipher_buf);
                send_msg_done(&nlh,cipher_buf, 256); 

                break;
            }
        case DECRYPT:
            {
                u8 *buf = ((u8*) NLMSG_DATA(nlh));
                u8 msg_buf[256];
                rsa_decrypt(buf, msg_buf);
                send_msg_done(&nlh, msg_buf, 256);

                break;
            }	
        case GEN_ENCRYPTED_KEY:
            {
                u8 *buf = ((u8*) NLMSG_DATA(nlh));
                u8 cipher_buf[256];
                rsa_gen_encrypted_key(buf, cipher_buf);
                send_msg_done(&nlh,cipher_buf, 256); 

                break;
            }
    }	
}
#endif

/* #ifdef KERNEL */ 
/* u32 __init rsaLoad(void) */
/* #else */
u32 rsaLoad(void)
/* #endif */
{
    u8 two_buf[CONFIG_RSA_AUX_OPERAND_SIZE];
    u8 one_buf[CONFIG_RSA_AUX_OPERAND_SIZE];

#ifdef KERNEL
    struct netlink_kernel_cfg cfg = {0};
#endif
    memset(two_buf,0,CONFIG_RSA_AUX_OPERAND_SIZE);
    memset(one_buf,0,CONFIG_RSA_AUX_OPERAND_SIZE);
    two_buf[CONFIG_RSA_AUX_OPERAND_SIZE-1] = 2;
    one_buf[CONFIG_RSA_AUX_OPERAND_SIZE-1] = 1;

    rsa_op_alloc(&zero,1);
    rsa_op_alloc(&one,1); rsa_op_set(&one,one_buf,sizeof(one_buf));
    rsa_op_alloc(&two,1); rsa_op_set(&two,two_buf,sizeof(two_buf));

#ifdef KERNEL    
    cfg.input = netlink_dispatcher;
    printk(KERN_INFO "rsa: creating netlink socket\n");
    nl_sk=netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    printk(KERN_INFO "rsa: netlink socket %d %d\n",nl_sk,NETLINK_USER);
#endif

    printk(KERN_INFO "rsa begin...\n");
    return 0;
}

/* #ifdef KERNEL */
/* u32 __exit rsaUnload(void) */
/* #else */
u32 rsaUnload(void)
/* #endif */
{
    //for (int i=0; i<AUX_OPERAND_COUNT; i++)
    //	rsa_op_free(aux[i]);
#ifdef KERNEL
    netlink_kernel_release(nl_sk);
#endif
    return 0;
}


int rsa_op_resize(rsa_op **n, int size)
{
    int retval = 0;
    rsa_op *handle = *n;

    /* If there is no allocated rsa_op passed in, allocate one */
    if (!handle)
    {	
        return rsa_op_alloc(n, size);
    }
    /* If the rsa_op passed in has the available limbs */
    if (handle->limbs >= size) {
        /* Zero the xtra limbs */
        if (size < handle->size)
            memset(handle->data + size, 0,
                    (handle->size - size) * sizeof(u32));
        handle->size = size;
    }
    else {

        rsa_op *tmp = NULL;
        retval = rsa_op_alloc(&tmp, size);

        if (retval < 0)
            return retval;
        if (handle->data != NULL) {
            /* Copy the original data */
            memcpy(tmp->data, handle->data, handle->size * sizeof(u32));
            tmp->sign = handle->sign;
        }
        //rsa_op_free(*n);
        *n = tmp;
    }

    return retval;
}

/* Count the leading zeroes of an operand */
static u32 rsa_op_clz(rsa_op *n)
{
    int i;
    u32 limb, retval = 0;

    for (i = n->size - 1; i >= 0; i--) {
        limb = n->data[i];

        if (!limb) {
            retval += 32;
            continue;
        }

        while (!(limb & U32_MSB_SET)) {
            retval++;
            limb = limb << 1;
        }
        break;
    }

    return retval;
}

char rsa_op_compare(rsa_op *l, rsa_op *r)
{
    int i;
    u32 *lbuf, *rbuf;

    /* Compare the two operands based on sign */
    if (l->sign != r->sign)
        return (l->sign)? -1: 1;

    /* Compare the two operands based on their size */
    if (l->size > r->size && rsa_op_clz(l) < (l->size - r->size) * 32)
        return 1;
    else if (r->size > l->size && rsa_op_clz(r) < (r->size - l->size) * 32)
        return -1;

    /* Compare the two operands based on their hex values */
    lbuf = l->data;
    rbuf = r->data;
    for (i = min(l->size, r->size) - 1; i >= 0; i--)
        if (lbuf[i] > rbuf[i])
            return 1;
        else if (lbuf[i] < rbuf[i])
            return -1;
    return 0;
}

void rsa_op_complement(rsa_op *n)
{
    int i, s;
    u32 *buf;

    s = n->size;
    buf = n->data;
    for (i = 0; i < s; i++)
        buf[i] = ~buf[i];

    /* Add 1 using the addition carry */
    for (i = 0; i < s; i++) {
        buf[i] += 1;
        if (buf[i])
            break;
    }
}


int rsa_op_shift(rsa_op **n, int bits)
{
    int i, distance, size, lz, retval;
    u32 *buf;
    rsa_op *handle;

    if (!bits)
        return 0;
    handle = *n;

    /* Shifting to the right, no resize needed */
    if (bits > 0) {
        /* Drop off one limb for each 32 bit shift */
        distance = bits / 32;
        size = handle->size;
        buf = handle->data;
        for (i = 0; i < size; i++)
            buf[i] = (i + distance >= size)? 0: buf[i + distance];

        /* Shift the remaining 'bits' mod 32 */
        bits = bits % 32;
        if (bits) {
            size -= distance;
            distance = 32 - bits;
            for (i = 0; i < size; i++) {
                buf[i] = buf[i] >> bits;
                if (i < size - 1)
                    buf[i] |=  buf[i + 1] << distance;
            }
        }

        rsa_op_canonicalize(handle);
        return 0;
    }

    bits = -bits;
    lz = rsa_op_clz(handle) + (handle->limbs - handle->size) * 32;

    /*
     * Shifting to the left.
     * Reallocation is needed when the leading zeroes are less than
     * the shift distance
     */
    if (lz < bits) {
        /* Compute the size of the reallocation */
        size = (bits - lz + 31) / 32;
        retval = rsa_op_resize(n, handle->limbs + size);
        if (retval < 0)
            return retval;
        handle = *n;
    }
    else
        handle->size += ((bits - rsa_op_clz(handle) + 31) / 32);

    buf = handle->data;
    distance = bits / 32;
    /* Shift data 1 byte to the left for each 32 bit shift */
    if (distance) {
        /* Shift bytes */
        for (i = handle->size - distance - 1; i >= 0; i--)
            buf[i + distance] = buf[i];

        /* Zero the shifted in bytes */
        memset(handle->data, 0, distance * sizeof(u32));
    }

    /* Shift the remaining 'bits' mod 32 */
    bits = bits % 32;
    distance = 32 - bits;
    if (bits)
        for (i = handle->size - 1; i >= 0; i--) {
            buf[i] = buf[i] << bits;
            if (i > 0)
                buf[i] |= (buf[i - 1] >> distance);
        }

    return 0;
}

/*
   + * Compute the modular inverse of an operand using only its 32 least
   + * significant bits (single presicion modular inverse)
   + */
u32 rsa_op_modinv32(rsa_op *n)
{
    u32 i, x, y, tmp, pow1;

    pow1 = y = 1;
    x = n->data[0];

    for (i = 2; i <= 32; i++) {
        pow1 = pow1 << 1;

        /*
         * This is the computation of x * y mod r, only r is a power
         * of 2, specifically 2^i so the modulo is computed by
         * keeping the least i bits of x * y.
         */
        tmp = ((u64)x * y) & (U32_MAX >> (32 - i));
        if (pow1 < tmp)
            y += pow1;
    }

    y = ~y + 1;
    return y;
}

int rsa_product(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux)
{
    int i, j, ls, rs, retval;
    u32 *buf, *lbuf, *rbuf;
    u64 tmp;
    rsa_op *handle;

    retval = rsa_op_copy(&LEFTOP, l);
    if (retval < 0)
        return retval;

    retval = rsa_op_copy(&RIGHTOP, r);
    if (retval < 0)
        return retval;

    ls = l->size;
    rs = r->size;
    //printk(KERN_INFO "%d %d\n",ls,rs);
    retval = rsa_op_resize(res, ls + rs);
    if (retval < 0)
        return retval;

    handle = *res;
    memset(handle->data, 0, handle->size * sizeof(u32));
    handle->sign = LEFTOP->sign ^ RIGHTOP->sign;

    buf = handle->data;
    lbuf = LEFTOP->data;
    rbuf = RIGHTOP->data;

    /* Make the multiplication, using the standard algorithm */
    for (i = 0; i < rs; i++) {
        tmp = 0;
        for (j = 0; j < ls; j++) {
            tmp = buf[i + j] + ((u64)lbuf[j] * (u64)rbuf[i]) + 
                (tmp >> 32);
            buf[i + j] = (u32)tmp;
        }

        buf[i + ls] = tmp >> 32;
    }

    rsa_op_canonicalize(handle);
    return 0;
}


int rsa_add(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux)
{
    int i, size, retval;
    u32 *buf, *lbuf, *rbuf;
    rsa_op *handle;

    retval = rsa_op_copy(&LEFTOP, l);
    if (retval < 0)
        return retval;

    retval = rsa_op_copy(&RIGHTOP, r);
    if (retval < 0)
        return retval;

    size = max(l->size, r->size) + (l->sign != r->sign);
    //printk(KERN_INFO "size = %d\n",size);
    retval = rsa_op_resize(res, size);
    if (retval < 0)
        return retval;

    //printk(KERN_INFO "size1 = %d\n",size);
    retval = rsa_op_resize(&LEFTOP, size);
    if (retval < 0)
        return retval;
    //printk(KERN_INFO "size2 = %d\n",size);
    retval = rsa_op_resize(&RIGHTOP, size);
    if (retval < 0)
        return retval;

    handle = *res;
    buf = handle->data;
    lbuf = LEFTOP->data;
    rbuf = RIGHTOP->data;

    /* If the operands are both negative or positive perform subtraction */
    if (LEFTOP->sign != RIGHTOP->sign) {
        bool borrow = false;
        u32 limb;

        for (i = 0; i < size; i++) {
            limb = borrow + rbuf[i];
            buf[i] = lbuf[i] - limb;
            borrow = (lbuf[i] < limb);
        }

        if (borrow) {
            rsa_op_complement(handle);
            handle->sign = !RIGHTOP->sign;
        }
        else
            handle->sign = RIGHTOP->sign;
    }
    /* If the operands are not signed in the same way perform addition */
    else {
        bool carry = false;
        u64 sum;

        for (i = 0; i < size; i++) {
            sum = lbuf[i] + rbuf[i] + carry;
            carry = (sum > U32_MAX);
            buf[i] = (u32)sum;
        }

        handle->sign = LEFTOP->sign;
    }

    rsa_op_canonicalize(handle);
    return 0;
}

int rsa_difference(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux)
{
    int i, size, retval;
    u32 *buf, *lbuf, *rbuf;
    rsa_op *handle;

    retval = rsa_op_copy(&LEFTOP, l);
    if (retval < 0)
        return retval;

    retval = rsa_op_copy(&RIGHTOP, r);
    if (retval < 0)
        return retval;

    size = max(l->size, r->size) + (l->sign != r->sign);
    retval = rsa_op_resize(res, size);
    if (retval < 0)
        return retval;

    retval = rsa_op_resize(&LEFTOP, size);
    if (retval < 0)
        return retval;
    retval = rsa_op_resize(&RIGHTOP, size);
    if (retval < 0)
        return retval;

    handle = *res;
    buf = handle->data;
    lbuf = LEFTOP->data;
    rbuf = RIGHTOP->data;

    /* If the operands are both negative or positive perform subtraction */
    if (LEFTOP->sign == RIGHTOP->sign) {
        bool borrow = 0;
        u32 limb;

        for (i = 0; i < size; i++) {
            limb = borrow + rbuf[i];
            buf[i] = lbuf[i] - limb;
            borrow = (lbuf[i] < limb);
        }

        if (borrow) {
            rsa_op_complement(handle);
            handle->sign = !RIGHTOP->sign;
        }
        else
            handle->sign = RIGHTOP->sign;
    }
    /* If the operands are not signed in the same way perform addition */
    else {
        bool carry = 0;
        u64 sum;

        for (i = 0; i < size; i++) {
            sum = (u64) lbuf[i] + (u64)rbuf[i] + carry;
            carry = (sum > U32_MAX);
            buf[i] = (u32)sum;
        }

        handle->sign = LEFTOP->sign;
    }

    rsa_op_canonicalize(handle);
    return 0;
}

int rsa_divide(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux)
{
    int k, retval=0;
    rsa_op* qoutent;
    rsa_op* zDiff;
    rsa_op_alloc(&qoutent,1);
    rsa_op_alloc(&zDiff,1);

    retval = rsa_op_copy(&LEFTOP, l);
    if (retval < 0)
        return retval;

    retval = rsa_op_copy(&RIGHTOP, r);
    if (retval < 0)
        return retval;

    k = (LEFTOP->size - RIGHTOP->size) * 32;
    k -= (rsa_op_clz(LEFTOP) - rsa_op_clz(RIGHTOP));

    /* Align the divisor to the dividend */
    if (k>0)
        retval = rsa_op_shift(&RIGHTOP, -k);

    if (retval < 0)
        return retval;
    while (rsa_op_compare(RIGHTOP,zero) > 0) {
        if (rsa_op_compare(LEFTOP,RIGHTOP) >=0) {
            do {
                rsa_add(&qoutent,qoutent,one,aux+2);
                rsa_difference(&LEFTOP,LEFTOP,RIGHTOP,aux+2);
            } while (rsa_op_compare(LEFTOP,RIGHTOP) >= 0);
        }
        rsa_op_shift(&RIGHTOP,1);
        if(rsa_op_compare(RIGHTOP,r) < 0) break;
        rsa_product(&qoutent,qoutent,two,aux+2);
    }
    rsa_op_canonicalize(qoutent);
    *res = qoutent;
    return retval;
}


int rsa_remainder(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op **aux)
{
    int i, k, retval;

    retval = rsa_op_copy(&LEFTOP, l);
    if (retval < 0)
        return retval;

    retval = rsa_op_copy(&RIGHTOP, r);
    if (retval < 0)
        return retval;

    k = (LEFTOP->size - RIGHTOP->size) * 32;
    k -= (rsa_op_clz(LEFTOP) - rsa_op_clz(RIGHTOP));

    /* Align the divisor to the dividend */
    //printk("about to right shift rightop...\n");
    //rsa_op_print(RIGHTOP,1);	
    //rsa_op_canonicalize(&RIGHTOP);
    if (k>0)	
        retval = rsa_op_shift(&RIGHTOP, -k);

    if (retval < 0)
        return retval;

    for (i = 0; i <= k; i++) {
        retval = rsa_difference(res, LEFTOP, RIGHTOP, aux + 2);
        if (retval < 0)
            return retval;

        if (!(*res)->sign) {
            retval = rsa_op_copy(&LEFTOP, *res);
            if (retval < 0)
                return retval;
        }
        retval = rsa_op_shift(&RIGHTOP, 1);
        if (retval < 0)
            return retval;
    }
    rsa_op_canonicalize(LEFTOP);
    return rsa_op_copy(res, LEFTOP);
}

/* Compute the montgomery product of two numbers */
int rsa_mproduct(rsa_op **res, rsa_op *l, rsa_op *r, rsa_op *n,
        u32 modinv, rsa_op **aux)
{
    int nsize, i, j, k, retval;
    u32 *buf, *nbuf, *tmp, m;
    u64 product = 0;

    nsize = n->size;
    //	printk(KERN_INFO "rsa: mont nsize = %d",nsize);
    k = nsize << 1;
    retval = rsa_product(res, l, r, aux);
    if (retval < 0)
        return retval;
    //printk(KERN_INFO "size11 = %d\n",max((*res)->size,k));
    retval = rsa_op_resize(res, max((*res)->size, k));
    if (retval < 0)
        return retval;

    tmp = buf = (*res)->data;
    nbuf = n->data;

    for (i = 0; i < nsize; i++, tmp++) {
        m = buf[i] * modinv;
        product = 0;

        for (j = 0; j < nsize; j++) {
            product = tmp[j] + (m * (u64)nbuf[j]) + (product >> 32);
            tmp[j] = (u32)product;
        }

        for (j = nsize + i; j < k; j++) {
            product = buf[j] + (product >> 32);
            buf[j] = (u32)product;
        }
    }
    //printk(KERN_INFO "size12 = %d\n",((*res)->size));
    retval = rsa_op_resize(res, (*res)->size + 1);
    if (retval < 0)
        return retval;

    (*res)->data[(*res)->size - 1] = product >> 32;
    retval = rsa_op_shift(res, nsize * 32);
    if (retval < 0)
        return retval;

    if (rsa_op_compare(*res, n) >= 0) {
        retval = rsa_difference(res, *res, n, aux);
        if (retval < 0)
            return retval;
    }
    return 0;
}


/**
  + * rsa_op_free - free the resources held by an rsa_op
  + * @n: pointer to the rsa_op to be freed
  + */
void rsa_op_free(rsa_op *n)
{
    if (!n)
        return;
#ifdef KERNEL
    vfree(n->data);
    vfree(n);
#else 
    free(n->data);
    free(n);
#endif
}


/**
  + * rsa_op_set - set the value of an rsa_op given its absolute value
  + * @n: pointer pointer to the allocated rsa_op
  + * @data: the value as a byte array
  + * @size: sizeof(data)
  + *
  + * The optional leading zeroes will be taken into account, so that the rsa_op
  + * created will not be canonicalized. The rsa_op passed in will be reallocated
  + * (and relocated) if needed
  + */
int rsa_op_set(rsa_op **n, u8 *data, u32 size)
{
    int s, i, j, retval;
    u32 *buf;

    if (size <= 0)
        return -EINVAL;

    /* Size of the new rsa_op value (in limbs) */
    s = (size + 3) / 4;
    //printk(KERN_INFO "size16 = %d\n",s);
    retval = rsa_op_resize(n, s);
    if (retval < 0)
        return retval;
    memset((*n)->data, 0, (*n)->size * sizeof(u32));

    /* Copy the data */
    buf = (*n)->data;
    for (i = size - 1, j = 0; i >= 0; i--, j++)
        buf[j / 4] |= ((u32)data[i] << ((j % 4) * 8));
    return 0;
}


/**
  + * rsa_op_copy - rsa_op to rsa_op copy
  + * @dest: destination rsa_op
  + * @src: source rsa_op
  + */
int rsa_op_copy(rsa_op **dest, rsa_op *src)
{
    int retval;
    retval = rsa_op_resize(dest, src->size);
    memset((*dest)->data,0,(*dest)->size * sizeof(u32));
    if (retval < 0)
        return retval;
    memcpy((*dest)->data, src->data, (src->size) * sizeof(u32));
    (**dest).sign = src->sign;
    return 0;
}


/**
  + * rsa_cipher - compute montgomery modular exponentiation, m^e mod n
  + * @res: pointer pointer to the allocated result
  + * @m: data to be ciphered, also base of the exponentiation
  + * @e: exponent
  + * @n: modulus
  + *
  + * The montgomery modular exponentiation is not generic. It can be used only
  + * in cases where the modulus is odd as it is with RSA public and private key
  + * pairs, and this is because the algorithm depends in such number properties
  + * to speed things up. The calculation results will be invalid if the modulus
  + * is even. Furthcipher(rsermore the base must be in the residue class of the modulus
  + * (m < n). The calculation result if m > n will be mathematically valid but
  + * will be useless for the RSA cryptosystem, because it will be the result of
  + * (m mod n)^e mod n and not m^e mod n. Also the bit width of the base must
  + * be equal to the modulus bit width.
  + */
int rsa_cipher(rsa_op **res, rsa_op *m, rsa_op *e, rsa_op *n)
{
    int i, j, retval;
    u32 limb, modinv;
    bool started = false;
    rsa_op *aux[AUX_OPERAND_COUNT], *M, *x, *tmp;

    /*
     * Check for bit width equality
     * Check if the base is in the residue class of the modulus
     * Check if the modulus is odd
     */
    if (m->size != n->size || rsa_op_compare(m, n) > 0 || !(n->data[0] & 1))
    {
        printk(KERN_INFO "rsa:not odd ! %d %d %d\n",m->size,n->size,(n->data[0] & 1));
        return -EINVAL;
    }
    modinv = rsa_op_modinv32(n);
    memset(&aux, 0, sizeof(aux));
    M = x = NULL;

    for (i = 0; i < AUX_OPERAND_COUNT; i++) {
        retval = rsa_op_alloc(&aux[i],
                CONFIG_RSA_AUX_OPERAND_SIZE);
        if (retval < 0)
            goto rollback;
    }

    /* Compute M = m*r mod n where r is 2^k and k is the bit length of n */
    retval = rsa_op_copy(&M, m);
    if (retval < 0)
        goto rollback;

    retval = rsa_op_shift(&M, -(n->size * 32));
    if (retval < 0)
        goto rollback;

    retval = rsa_remainder(&M, M, n, aux);
    if (retval < 0)
        goto rollback;

    /* Compute x = r mod n where r is 2^k and k is the bit length of n */
    retval = rsa_op_init(&x, "\x01", 1, 32);
    if (retval < 0)
        goto rollback;

    retval = rsa_op_shift(&x, -(n->size * 32));
    if (retval < 0)
        goto rollback;

    retval = rsa_remainder(&x, x, n, aux);
    if (retval < 0)
        goto rollback;

    /*
     * Canonicalize the exponent and compute the modular exponentiation.
     * For each limb of the exponent perform left to right binary
     * exponentiation
     */
    rsa_op_canonicalize(e);
    for (i = e->size - 1; i >= 0; i--) {
        /* Take a limb of the exponent */
        limb = e->data[i];

        /* For each of its bits */
        for (j = 0; j < 32; j++) {
            /*
             * While the exponent has non significant zeroes shift
             * it to the left
             */
            if (!(limb & U32_MSB_SET) && !started) {
                limb = limb << 1;
                continue;
            }
            started = true;

            /* Compute x = x*x mod n */
            retval = rsa_mproduct(&x, x, x, n, modinv, aux);
            if (retval < 0)
                goto rollback;

            if (limb & U32_MSB_SET) {
                /* Compute x = M*x mod n */
                retval = rsa_mproduct(&x, x, M, n, modinv, aux);
                if (retval < 0)
                    goto rollback;
            }

            limb = limb << 1;
        }
    }

    /* Compute res = x mod n */
    retval = rsa_op_init(&tmp, "\x01", 1, 32);
    if (retval < 0)
        goto rollback;
    retval = rsa_mproduct(res, x, tmp, n, modinv, aux);

    /* Free all allocated resources */
rollback:
    for (i = 0; i < AUX_OPERAND_COUNT; i++)
        rsa_op_free(aux[i]);
    rsa_op_free(M);
    rsa_op_free(x);
    rsa_op_free(tmp);
    return retval;
}

void rsa_generate_n (rsa_op** res, rsa_op* p, rsa_op* q) {
    rsa_op* n;
    int i;
    rsa_op *aux[AUX_OPERAND_COUNT];

    memset(&aux,0,sizeof(aux));
    for (i = 0; i<8; i++) {
        rsa_op_alloc(&(aux[i]),32);
    }

    rsa_op_alloc(&n,CONFIG_RSA_AUX_OPERAND_SIZE);
    rsa_product(&n,p,q,aux);

    *res = n;
}

void rsa_generate_fi_n(rsa_op** res, rsa_op* p, rsa_op* q) {
    rsa_op* n;
    rsa_op* p1;
    rsa_op* q1;
    rsa_op *aux[AUX_OPERAND_COUNT];
    int i;

    memset(&aux,0,sizeof(aux));
    for (i = 0; i<8; i++) {
        rsa_op_alloc(&(aux[i]),32);
    }

    rsa_op_alloc(&p1,1);
    rsa_op_alloc(&q1,1);
    rsa_op_alloc(&n,1);

    rsa_difference(&p1,p,one,aux);

    rsa_difference(&q1,q,one,aux);

    rsa_product(&n,p1,q1,aux);

    rsa_op_free(p1);
    rsa_op_free(q1);

    *res = n;
}


int rsa_op_abs(rsa_op** dest, rsa_op* src) {
    int retval;
    retval = rsa_op_resize(dest, src->size);
    if (retval < 0)
        return retval;
    memcpy((*dest)->data, src->data, (src->size) * sizeof(u32));
    return 0;
}

int rsa_op_modular_inverse(rsa_op** res, rsa_op* e, rsa_op* fin, rsa_op** aux)
{
    rsa_op *q,*t,*x0,*x1,*b,*a;
    rsa_op_alloc(&t,1);rsa_op_alloc(&b,1);rsa_op_alloc(&a,1);
    rsa_op_copy(&b,fin);
    rsa_op_copy(&a,e);
    rsa_op_alloc(&q,1);rsa_op_alloc(&x0,1);rsa_op_init(&x1,"\x01",1,1);
    //memset(x0->data,0,32);
    while (rsa_op_compare(a,one)>0) {
        rsa_divide(&q,a,b,aux+2);
        rsa_op_copy(&t,b);rsa_remainder(&b,a,b,aux);rsa_op_copy(&a,t);
        rsa_op_copy(&t,x0);
        rsa_product(&x0,x0,q,aux);
        rsa_difference(&x0,x1,x0,aux);
        rsa_op_copy(&x1,t);
    }
    if (x1->sign) {
        printk("negative x\n");
        rsa_difference(&x1,zero,x1,aux+2);
        rsa_difference(&x1,fin,x1,aux+2);
    }

    rsa_op_canonicalize(x1);
    *res = x1;

    return 0;
}


int rsa_op_extended_euclidean_inverse(rsa_op** res, rsa_op* e, rsa_op* fi_n) {

    rsa_op* ri;
    rsa_op* qi;
    rsa_op* yi1; //y(i-1)
    rsa_op* yi2; //y(i-2)
    rsa_op* tmpy;
    rsa_op* aux[AUX_OPERAND_COUNT];
    int i,retval;

    memset(&aux, 0, sizeof(aux));

    for (i = 0; i < AUX_OPERAND_COUNT; i++) {
        retval = rsa_op_alloc(&aux[i],
                CONFIG_RSA_AUX_OPERAND_SIZE/8);
        if (retval < 0)
            return -1;
    }

    rsa_op_alloc(&tmpy,1);
    rsa_op_alloc(&ri,1);
    rsa_op_alloc(&qi,1);
    rsa_op_alloc(&yi1,1);
    rsa_op_alloc(&yi2,1);

    rsa_op_copy(&yi1,one);
    rsa_op_copy(&LEFTOP,fi_n);
    rsa_op_copy(&RIGHTOP,e);
    rsa_remainder(&ri,LEFTOP,RIGHTOP,aux+2);	

    while (rsa_op_compare(ri,one) > 0) {
        rsa_remainder(&ri,LEFTOP,RIGHTOP,aux+2);
        rsa_divide(&qi,LEFTOP,RIGHTOP,aux+2);

        rsa_op_copy(&LEFTOP,RIGHTOP);
        rsa_op_copy(&RIGHTOP,ri);

        rsa_product(&tmpy,qi,yi1,aux+2);
        rsa_difference(&tmpy,yi2,tmpy,aux+2);

        rsa_op_copy(&yi2,yi1);
        rsa_op_copy(&yi1,tmpy);
    }

    if (yi1->sign)
    {
        printk("fix the sign\n");
        //rsa_op_abs(&yi1,yi1);
        yi1->sign = 0; 
        rsa_difference(&yi1,fi_n,yi1,aux+2);
    } 
    *res = yi1;
    printk("last remainder:\n");rsa_op_print(ri,1);

    return 0;

}

//generates 1024bit size random prime using /dev/urandom or get_random_bytes (if in kernel)
int rsa_op_random_prime_generator(rsa_op** n, u32 byte_size) {
    int i;
    rsa_op* op;
    rsa_op* res;
    rsa_op* prime;
    rsa_op* tmp;
#ifndef KERNEL
    int randptr;
#endif
    unsigned char buf[byte_size];
    rsa_op *aux[AUX_OPERAND_COUNT];
    rsa_op_alloc(&tmp,1);

    memset(&aux,0,sizeof(aux));
    for (i = 0; i<8; i++) {
        rsa_op_alloc(&(aux[i]),byte_size/8);
    }

#ifndef KERNEL
    randptr = open("/dev/urandom", O_RDONLY);
#endif
    rsa_op_alloc(&op,byte_size/8);
    rsa_op_alloc(&res,byte_size/8);
    rsa_op_alloc(&prime,byte_size/8);
    memset(buf,0,sizeof(buf));
    i=0;
    do {
#ifndef KERNEL
        read(randptr,buf,sizeof(buf));
#else
        get_random_bytes(buf,sizeof(buf));
#endif
        buf[0] |= 0b10000000;
        buf[byte_size-1] |= 0b00000001;
        rsa_op_set(&prime,buf,sizeof(buf));
        rsa_op_alloc(&op,1);
        rsa_difference(&op,prime,one,aux);
        rsa_op_resize(&two,prime->size); 
        rsa_cipher(&tmp,two,op,prime);
    } while (rsa_op_compare(tmp,one) != 0);
#ifndef KERNEL
    close(randptr);
#endif
    rsa_op_free(op);
    *n = prime;

    return i;
}

void rsa_key_generation(rsa_ctx* ctx) {
    rsa_op* p;
    rsa_op* q;
    rsa_op* fi_n;
    u8 buf[3], bufp[CONFIG_RSA_AUX_OPERAND_SIZE];
    rsa_op *aux[AUX_OPERAND_COUNT];
    int i = 0;

    memset(&aux,0,sizeof(aux)); 
    memset(bufp,0,sizeof(bufp));
    for (i = 0; i<AUX_OPERAND_COUNT; i++) {
        rsa_op_alloc(&(aux[i]),32);
    }

    rsa_op_alloc(&p,1);
    rsa_op_alloc(&q,1);

    rsa_op_random_prime_generator(&p,128);
    rsa_op_random_prime_generator(&q,128);

    rsa_op_alloc(&(ctx->d),128); 
    rsa_op_alloc(&(ctx->n),128);

    rsa_op_alloc(&fi_n,1);
    rsa_product(&(ctx->n),p,q,aux);
    rsa_generate_fi_n(&fi_n,p,q);

    memset(buf,0,sizeof(buf));
    buf[0] = 1; buf[2] = 1; // 2^16+1
    rsa_op_init(&(ctx->e),buf,sizeof(buf),0);
    rsa_op_canonicalize(ctx->e);
    rsa_remainder(&p,fi_n,ctx->e,aux);
    if (rsa_op_compare(p,zero) == 0) {
        printk("error choosing e, try again \n"); return;
    }
    rsa_op_modular_inverse(&(ctx->d),ctx->e,fi_n,aux);
}

#ifdef KERNEL

EXPORT_SYMBOL(rsa_op_alloc);
EXPORT_SYMBOL(rsa_op_free);
EXPORT_SYMBOL(rsa_op_init);
EXPORT_SYMBOL(rsa_op_set);
EXPORT_SYMBOL(rsa_op_copy);
EXPORT_SYMBOL(rsa_op_print);
EXPORT_SYMBOL(rsa_cipher);
EXPORT_SYMBOL(rsa_op_random_prime_generator);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tasos Parisinos @ Sciensis Advanced Technology Systems & Asaf Algawi @ Tel Aviv College School of Computer Science");
MODULE_DESCRIPTION("RSA cipher algorithm implementation");
MODULE_ALIAS("rsa");

#ifndef TPVM
module_init(rsaLoad);
module_exit(rsaUnload);
#endif

#endif
