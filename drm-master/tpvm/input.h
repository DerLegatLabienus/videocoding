#ifndef __INPUT_H__
#define __INPUT_H__

#define NAL_BUF_SIZE  65536  // maximum NAL unit size
#define RING_BUF_SIZE  8192  // input ring buffer size, MUST be a power of two!
#define RING_BUF_HALF_SIZE  (RING_BUF_SIZE / 2)

#include <linux/fs.h>

int input_open(const unsigned char *filename);
void input_rewind(void);
int input_read(unsigned char *dest, int size);
void input_close(void);

int input_peek_bits(int bit_count);
void input_step_bits(int bit_count);
int input_get_bits(int bit_count);

int input_get_one_bit(void);

int input_byte_aligned(void);
void input_align_to_next_byte(void);
int input_get_byte(void);

#endif /*__INPUT_H__*/
