#include "common.h"
#include "input.h"

extern int input_size;
extern int input_remain;
extern int ring_pos;
extern unsigned char ring_buf[RING_BUF_SIZE];

#define HALF_RING ((RING_BUF_SIZE)/2)
unsigned char src[HALF_RING];

int input_open(char *filename) {
  input_size=4;
  input_rewind();
  return input_size;
}

int input_read(unsigned char *dest, int size) {
  memcpy(dest,src,size);
  input_remain+=size;
  return size;
}

void input_rewind() {
  input_remain=0;
  input_read(ring_buf,HALF_RING);
  input_read(&ring_buf[HALF_RING],HALF_RING);
  ring_pos=0;
}

void input_close() {
  input_size=0;
}
