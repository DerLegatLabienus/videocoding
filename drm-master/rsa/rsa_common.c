#include "rsa_common.h"

#ifdef KERNEL
#include <linux/vmalloc.h>
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

#define ENOMEM -1
#define EINVAL -1
#define true    1
#define false   0
#define printk printf
#define KERN_INFO

#endif

#define u8   unsigned char
#define u32  unsigned int
#define u64  unsigned long


/**
+ * rsa_op_alloc - allocate an rsa_op
+ * @n: pointer pointer to the allocated rsa_op
+ * @limbs: number of allocated limbs (32 bit digits)
+ *
+ * The allocated rsa_op will be zeroed and not canonicalized
+ */
int rsa_op_alloc(rsa_op **n, int limbs)
{
	rsa_op *handle;

	*n = NULL;
	if (!limbs)
		return -EINVAL;
	
	/* Allocate space for the rsa_op */
#ifdef KERNEL
	handle = vmalloc(sizeof(rsa_op));
#else
	handle = malloc(sizeof(rsa_op));
#endif
	if (!handle)
		return -ENOMEM;

	/* Allocate space for its data */
#ifdef KERNEL
	handle->data  = vmalloc(limbs * sizeof(u32));
	memset(handle->data, 0, limbs * sizeof(u32));
#else
	handle->data = calloc(limbs, sizeof(u32));
#endif
	if (!handle->data) {
#ifdef KERNEL
		kfree(handle);
#else
		free(handle);
#endif
		return -ENOMEM;
	}

	handle->sign = 0;
	handle->size = handle->limbs = limbs;
	*n = handle;
	return 0;
}

/**
+ * rsa_op_init - initialize an rsa_op given its absolute value
+ * @n: pointer pointer to the allocated rsa_op
+ * @data: the value as a byte array
+ * @size: sizeof(data)
+ * @xtra: how many extra limbs to preallocate to avoid reallocations
+ *
+ * The optional leading zeroes will be taken into account, so that the
+ * rsa_op created will not be canonicalized
+ */
int rsa_op_init(rsa_op **n, u8 *data, u32 size, u32 xtra)
{
	int i, j, s, retval;
	u32 *buf;

	*n = NULL;
	if (!size && !xtra)
		return -EINVAL;

	/* Allocate space for the rsa_op and its data */
	s = (size + 3) / 4;
	retval = rsa_op_alloc(n, s + xtra);
	if (retval < 0)
		return retval;

	(*n)->size = s;
	buf = (*n)->data;

	/* Copy the data */
	for (i = size - 1, j = 0; i >= 0; i--, j++)
		buf[j / 4] |= ((u32)data[i] << ((j % 4) * 8));
	return 0;
}

/**
+ * rsa_op_print - print the value of an rsa_op
+ * @n: pointer to the rsa_op to be printed
+ * @how: true to print canonicalized
+ */
void rsa_op_print(rsa_op *n, unsigned int how)
{
	int i, j;
	u32 limb;
	u8 byte;
	unsigned int started = false;

	printk("Operand @ 0x%x, %d limbs in size, %d limbs allocated, value = ",
	       (u32)n, n->size, n->limbs);

	/* Print the sign */
	printk("%s", (n->sign)? "-": " ");

	/* Print the hex value */
	for (i = n->size - 1; i >= 0; i--) {
		limb = n->data[i];

		/* Ignore leading zero limbs if canonicalization is selected */
		if (!limb && !started && how)
			continue;

		/*
		 * Print each limb as though each of its nibbles was a character
		 * from the set '0' to '9' and 'a' to 'f'
		 */
		for (j = 28; j >= 0; j -= 4) {
			byte = (u8)((limb >> j) & 0x0F);

			/*
			 * Ignore leading zero bytes if canonicalization
			 * is selected
			 */
			if (!byte && !started && how)
				continue;

			started = true;
			byte += (byte <= 0x09)? '0': 'a' - 0x0A;
			printk("%c", byte);
		}
	}

	if(!started)
		printk("0");
	printk("\n");
}

static void reverse(u8* buf, int startIndex, int endIndex)
{
    u8 tmp;
    int i,j;
	for (i = startIndex,j = endIndex; i<j; i++,j--) {
		tmp    = buf[i];
		buf[i] = buf[j];
		buf[j] = tmp;
	}
}

int rsa_op_serialize(rsa_op* op, u8* buf,int startIndex)
{
	int i,j,z;
	printk (KERN_INFO "rsa: serializing op of size %d at startindex %d\n", op->size,startIndex);
	for (i=0, z = startIndex; i<op->size; i++) {
		buf[z + i*4]   = (u8) (op->data[i]);
		buf[z + i*4+1] = (u8) (op->data[i]>>8);
		buf[z + i*4+2] = (u8) (op->data[i]>>16);
		buf[z + i*4+3] = (u8) (op->data[i]>>24);
	}

	j = z+i*4-1;
	reverse(buf, startIndex, j);
	return j+startIndex;
}

