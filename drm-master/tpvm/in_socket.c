#include "common.h"
#include "input.h"

extern int input_size;
extern int input_remain;
extern int ring_pos;
extern unsigned char ring_buf[RING_BUF_SIZE];

extern struct eventfd_ctx *ctx_k2l, *ctx_l2k; 

int fill_buffer() {
    //buffer_read_pos = 0;
    return 0;
}

int input_open(char *ignored) {
    fill_buffer();
    input_rewind();
    return 1;
}

int input_read(unsigned char *dest, int size) {
    int totalread = 0;
    int remain = BUFFER_SIZE - buffer_read_pos;
    if (remain < size) {
        memcpy(dest, buffer + buffer_read_pos, remain);
        dest += remain;
        size -= remain;
        totalread = remain;
        fill_buffer();
    }
    memcpy(dest, buffer + buffer_read_pos, size);
    buffer_read_pos += size;
    totalread += size;
    input_remain += totalread;
    return totalread;
}

void input_rewind() {
    input_remain=0;
    input_read(ring_buf,RING_BUF_SIZE);
    ring_pos=0;
}

void input_close() {
    input_size=0;
}

