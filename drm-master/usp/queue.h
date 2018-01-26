#ifndef QUEUE_H
#define QUEUE_H

struct queue_root;

struct queue_node {
    unsigned char data[4096];
    int data_size;
    struct queue_node *next;
};

struct queue_root *alloc_queue_root();
void init_queue_node(struct queue_node *head);

void queue_put(struct queue_node *new, struct queue_root *root);

struct queue_node *queue_get(struct queue_root *root);

#endif // QUEUE_H
