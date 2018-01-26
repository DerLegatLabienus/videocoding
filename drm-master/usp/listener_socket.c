#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "queue.h"
#include "listener.h"
#include "input.h"

#define PORT "3490"  // the port users will be connecting to

const int yes = 1;

extern struct queue_root* root;

void sigchld_handler(int s)
{
    (void)s;

    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int client_fd;

int setup_server()
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    struct addrinfo *servinfo;
    int rv;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    int sockfd;
    struct addrinfo *p;
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, 1) == -1) {
        perror("listen");
        return -1;
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    printf("server: waiting for client connection on port %s...\n", PORT);

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size = sizeof(their_addr);
    client_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (client_fd == -1) {
        perror("accept");
        return -1;
    }

    printf("server: client connected, client_fd = %d\n", client_fd);

    return 0;
}

void* listener_thread(void* ignored)
{
    (void)ignored;

    if (!client_fd) {
        fprintf(stderr, "client_fd is invalid\n");
        return NULL;
    }

    struct queue_node* node = NULL;
    int res = 0, bytes_read = 0;
    for(;;) {
        node = malloc(sizeof(struct queue_node));
        init_queue_node(node);
        bytes_read = 0;
        while (bytes_read < RING_BUF_HALF_SIZE) {
            res = read(client_fd, node->data + bytes_read, RING_BUF_HALF_SIZE - bytes_read);
            if (res < 1) {
                break;
            }
            bytes_read += res;
        }
        if (!bytes_read) {
            free(node);
            break;
        }
        node->data_size = bytes_read;
        queue_put(node, root);
    }

    close(client_fd);  // parent doesn't need this

    return NULL;
}
