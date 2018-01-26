/* deprecated - use in_queue.c */

#include "common.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "7890"    // the port users will be connecting to

extern int input_size;
extern int input_remain;
extern int ring_pos;
extern unsigned char ring_buf[RING_BUF_SIZE];

#define BUFFER_SIZE (4096*4)
unsigned char buffer[BUFFER_SIZE];
unsigned char* buffer_end = buffer + BUFFER_SIZE;
unsigned int buffer_read_pos;

int sockfd;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int fill_buffer() {
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    unsigned char* curr = buffer;

    if(!sockfd) {
        fprintf(stderr,"fill_buffer called, but no socket opened!\n");
        return -1;
    }

    while (curr < buffer_end) {
        if ((numbytes = recvfrom(sockfd, curr, buffer_end - curr , 0,
                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        curr += numbytes;
    }

    buffer_read_pos = 0;
    return 0;
}

int input_open(char *ignored) {

    struct addrinfo hints, *servinfo, *p;
    int rv;

    sockfd = 0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 0;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 0;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");
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
    if(!sockfd) {
        fprintf(stderr,"input_rewind called, but no socket opened!\n");
        return;
    }
    /* fseek(input_fd,0,SEEK_SET); */
    input_remain=0;
    input_read(ring_buf,RING_BUF_SIZE);
    ring_pos=0;
}

void input_close() {
    if (!sockfd) return;
    close(sockfd);
    input_size=0;
    sockfd=0;
}

