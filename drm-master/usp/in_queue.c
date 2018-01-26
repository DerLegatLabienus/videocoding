#include "common.h"
#include "input.h"
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <pthread.h>
#include <sched.h>

extern int input_size;
extern int input_remain;
extern int ring_pos;
extern unsigned char ring_buf[RING_BUF_SIZE];

struct queue_root* root;

int input_open(char *ignored) {
    (void)ignored;
    input_rewind();

    return 1;
}

int input_read(unsigned char *dest, int size) {
    assert(size == RING_BUF_SIZE >> 1);
    struct queue_node* node = NULL;
    while (!(node = queue_get(root))) {
        if (sched_yield() < 0) {
            perror("sched_yield");
            return -1;
        }
    }
    const int to_read = (size < node->data_size) ? size : node->data_size;
    memcpy(dest, node->data, to_read);
    input_remain += to_read;
    free(node);
    return to_read;
}

void input_rewind() {
    input_remain = 0;
    input_read(ring_buf,                      RING_BUF_HALF_SIZE);
    input_read(ring_buf + RING_BUF_HALF_SIZE, RING_BUF_HALF_SIZE);
    ring_pos=0;
}

void input_close() {
}
