#ifndef __QUEUE_H__
#define __QUEUE_H__

struct queue_root;

typedef struct queue_node {
    unsigned char data[4096];
    int data_size;
    struct queue_node *next;
} queue_node;

struct queue_root *alloc_queue_root(void);
void init_queue_node(struct queue_node *head);
queue_node* create_empty_node(void);

/* will return -1 if couldn't put the new node (the semaphore was interrupted), 0 if succeded */
int queue_put(struct queue_node *new, struct queue_root *root);

struct queue_node *queue_get(struct queue_root *root);

#endif // QUEUE_H
