#include "queue.h"
#include "listener.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

extern struct queue_root* root;

char* path = "../streams/Aladin.264";

int setup_server() {
    return 0;
}

void* listener_thread(void* arg_path)
{
    if (arg_path)
        path = arg_path;
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "couldn't open %s\n", path);
        return NULL;
    }
    struct queue_node* node = NULL;
    int res = 0;
    for(;;) {
        node = malloc(sizeof(struct queue_node));
        init_queue_node(node);
        res = fread(node->data, 1, RING_BUF_HALF_SIZE, file);
        if (!res) {
            free(node);
            break;
        }
        node->data_size = res;
        queue_put(node, root);
    }
    fclose(file);
    return NULL;
}
