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
#define HOSTNAME "127.0.0.1"
#define FILENAME "../streams/Aladin.264"

FILE *input_fd=NULL;
struct addrinfo *servinfo, *p;
int input_size, input_read;

int input_open(char *filename) {
    if(input_fd) {
        fprintf(stderr,"input_open: file already opened\n");
        return 0;
    }
    input_fd=fopen(filename,"rb");
    if(!input_fd) {
        perror("input_open: cannot open file");
        return 0;
    }
    fseek(input_fd,0,SEEK_END);
    input_size=ftell(input_fd);
    fseek(input_fd,0,SEEK_SET);
    input_read=0;
    return input_size;
}

void input_close() {
    if(!input_fd) return;
    fclose(input_fd);
    input_size=0;
    input_fd=NULL;
}

int sock_connect(char hostname[], char port[])
{
    int sockfd;
    struct addrinfo hints;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to bind socket\n");
        return 2;
    }

    return sockfd;
}

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    char buffer[BUFFER_SIZE];
    int sockfd = sock_connect(HOSTNAME, PORT);
    int numbytes;
    input_size = input_open(FILENAME);
    unsigned int usecs = 5000;
    if (argc > 1) {
        usecs = (unsigned int)atoi(argv[1]);
    }
    printf("usecs = %d\n", usecs);

    while (input_read < input_size)
    {
        int count=fread(buffer,1,BUFFER_SIZE,input_fd);
        int sent=0;
        while (sent < count) {
            if ((numbytes = sendto(sockfd, buffer, count, 0,
                            p->ai_addr, p->ai_addrlen)) == -1) {
                perror("talker: sendto");
                exit(1);
            }
            sent += numbytes;
        }
        input_read += count;
        usleep(usecs);
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    input_close();

    return 0;
}
