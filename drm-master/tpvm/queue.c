#include <linux/semaphore.h>
#include <linux/slab.h>
#include "queue.h"
 
#define QUEUE_POISON1 ((void*)0xCAFEBAB5)
 
struct queue_root {
    struct queue_node *head;
    struct semaphore head_sem;
 
    struct queue_node *tail;
    struct semaphore tail_sem;
 
    struct queue_node divider;
};
 
struct queue_root *alloc_queue_root(void)
{
    struct queue_root *root = kmalloc(sizeof(struct queue_root), GFP_KERNEL);
    sema_init(&root->head_sem, 1);
    sema_init(&root->tail_sem, 1);
 
    root->divider.next = NULL;
    root->head = &root->divider;
    root->tail = &root->divider;
    return root;
}

queue_node* create_empty_node(void) {
    queue_node* node = kmalloc(sizeof(struct queue_node), GFP_KERNEL);
    init_queue_node(node);
    memset(node->data, 0, 4096);
    node->data_size = 0;
    return node;
}
 
void init_queue_node(struct queue_node *head)
{
    head->next = QUEUE_POISON1;
}
 
int queue_put(struct queue_node *new, struct queue_root *root)
{
    new->next = NULL;
 
    if (down_interruptible(&root->tail_sem) != 0)
        return -1;
    root->tail->next = new;
    root->tail = new;
    up(&root->tail_sem);
    return 0;
}
 
struct queue_node *queue_get(struct queue_root *root)
{
    struct queue_node *head, *next;
    if (!root) {
        return NULL;
    }
 
    while (1) {
        if (down_interruptible(&root->head_sem) != 0)
            return NULL;
        head = root->head;
        next = head->next;
        if (next == NULL) {
            up(&root->head_sem);
            return NULL;
        }
        root->head = next;
        up(&root->head_sem);
         
        if (head == &root->divider) {
            queue_put(head, root);
            continue;
        }
 
        head->next = QUEUE_POISON1;
        return head;
    }
}
