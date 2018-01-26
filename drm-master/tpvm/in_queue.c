#include <linux/slab.h>
#include "input.h"
#include <linux/sched.h>
#include "queue.h"

extern int input_size;
extern int input_remain;
extern int ring_pos;
extern unsigned char ring_buf[RING_BUF_SIZE];

struct queue_root* root;

int input_open(const unsigned char *ignored) {
    (void)ignored;

    input_rewind();

    return 1;
}

int input_read(unsigned char *dest, int size) {
    int read = 0;
    struct queue_node* node = NULL;
    //printk(KERN_INFO "in input_read, waiting for queue_get()... ");
    while (!(node = queue_get(root))) {
        schedule();
    }
    read = (size < node->data_size) ? size : node->data_size;
    memcpy(dest, node->data, read);
    kfree(node);
    //printk(KERN_INFO "in input_read, read = %d\n", read);
    input_remain += read;                               
    return read;
}

void input_rewind() {
    input_remain = 0;
    input_read(ring_buf,                      RING_BUF_HALF_SIZE);
    input_read(ring_buf + RING_BUF_HALF_SIZE, RING_BUF_HALF_SIZE);
    ring_pos=0;
}

void input_close() {
}
