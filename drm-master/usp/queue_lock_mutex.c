#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
 
#include "queue.h"
 
#define QUEUE_POISON1 ((void*)0xCAFEBAB5)
 
struct queue_root {
    struct queue_node *head;
    pthread_mutex_t head_lock;
 
    struct queue_node *tail;
    pthread_mutex_t tail_lock;
 
    struct queue_node divider;
};
 
struct queue_root *alloc_queue_root()
{
    struct queue_root *root = malloc(sizeof(struct queue_root));
    pthread_mutex_init(&root->head_lock, NULL);
    pthread_mutex_init(&root->tail_lock, NULL);
 
    root->divider.next = NULL;
    root->head = &root->divider;
    root->tail = &root->divider;
    return root;
}
 
void init_queue_node(struct queue_node *head)
{
    head->next = QUEUE_POISON1;
}
 
void queue_put(struct queue_node *new, struct queue_root *root)
{
    new->next = NULL;
 
    pthread_mutex_lock(&root->tail_lock);
    root->tail->next = new;
    root->tail = new;
    pthread_mutex_unlock(&root->tail_lock);
}
 
struct queue_node *queue_get(struct queue_root *root)
{
    struct queue_node *head, *next;
    if (!root) {
        fprintf(stderr, "queue_get called with NULL root!");
        return NULL;
    }
 
    while (1) {
        pthread_mutex_lock(&root->head_lock);
        head = root->head;
        next = head->next;
        if (next == NULL) {
            pthread_mutex_unlock(&root->head_lock);
            return NULL;
        }
        root->head = next;
        pthread_mutex_unlock(&root->head_lock);
         
        if (head == &root->divider) {
            queue_put(head, root);
            continue;
        }
 
        head->next = QUEUE_POISON1;
        return head;
    }
}
