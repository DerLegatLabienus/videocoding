#include "fetcher.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include "common.h"
#include "../rsa/rsa_common.h"
#define NAME "fetcher"

int efd_k2l = 0; 
int efd_l2k = 0;
int dev_fd  = -1;

const char PORT[] = "7890";
//const char HOST[] = "192.168.56.1";
const char HOST[] = "localhost";

#define BUFFER_SIZE (BLK_SIZE * BLKS)
#define TCP 1
#define KEY_LEN 256
#define VIDEO_ID_LEN 256

unsigned char tpvm_key[KEY_LEN];

unsigned char buffer[BUFFER_SIZE];
unsigned char* buffer_end = buffer + BUFFER_SIZE;
unsigned int buffer_read_pos;

unsigned char* bufs[2];
int blk_id = 0;
int server = -1; // a socket to the server
int interrupted = 0;

void cleanup(int send_abort) {
    if (send_abort && efd_l2k) {
        static uint64_t abort = ABORT;
        write(efd_l2k, &abort, sizeof(abort));
    }
    if (server > 0) {
        close(server);
        server = 0;
    }
    if (dev_fd  > 0) {
        close(dev_fd);
    }
    if (efd_k2l > 0) {
        close(efd_k2l);
    }
    if (efd_l2k > 0) {         
        close(efd_l2k);
    }
}

static void handle_int(int num) {
    PRINT(("handle_int called with %d\n", num));
    cleanup(1);
}

int setup_efds() {
    efd_k2l = eventfd(0,0);
    efd_l2k = eventfd(0,0);
    return (efd_k2l && efd_l2k) ? 0 : -1;
}

int write_metadata(Metadata metadata, const char* path)
{
    const int ep_fd = open(path, O_WRONLY);
    if(ep_fd < 0) {
        fprintf(stderr, "Failed opening %s (%s)! Exiting...\n", path, strerror(errno));
        return -1;
    }
    if (write(ep_fd, &metadata, sizeof(metadata)) < 0) {
        fprintf(stderr, "write to %s failed (%s)!\n", path, strerror(errno));
        close(ep_fd);
        return -1;
    }
    close(ep_fd);
    return 0;
}

// setup eventfds, writes them to the kernel, mmaps kernel memory and return its address
unsigned char* setup_kernel_context(Metadata metadata) {
    const char tpvm_listener_info_path[] = "/proc/tpvm_listener_info";

    printf( "1\n");
    if (setup_efds() < 0 || 
            write_efds(efd_k2l, efd_l2k, tpvm_listener_info_path) < 0 || 
            write_metadata(metadata, "/proc/tpvm_metadata") < 0) {
        return NULL;
    }

    printf( "2\n");
    dev_fd = open(dev_path, O_RDWR);
    if (dev_fd <= 0) {
        fprintf(stderr, "failed opening device %s! exiting\n", dev_path);
        return NULL;
    }

    printf( "3\n");
    if (ioctl(dev_fd, KEYJ_IOCLISTENER, 0) < 0) {
        fprintf(stderr, "failed invocking ioctl, exiting\n");
        return NULL;
    }

    printf( "4\n");
    unsigned char* addr = mmap(NULL, BLKS * BLK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0); 
    if (addr == MAP_FAILED) {
        fprintf(stderr, "mmap failed, exiting\n");
        return NULL;
    }

    return addr;
}

int socket_to_server(int socktype) {

    struct addrinfo hints, *servinfo, *p;
    int rv;

    if (socktype != SOCK_DGRAM && socktype != SOCK_STREAM)
        return -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = socktype;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(HOST, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    int sock = -1;
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror(NAME ": socket");
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock);
            perror(NAME ": connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, NAME ": failed to bind socket\n");
        return -1;
    }

    freeaddrinfo(servinfo);

    return sock;
}

int handle_block(int blk_size) {
    static uint64_t kernel_state       = NA;
    static uint64_t new_data_available = NA;
    static uint64_t video_done         = PACK(VIDEO_DONE, 0);
    static uint64_t abort              = PACK(ABORT, 0);

    if (blk_size == 0) {
        PRINT(("blk_size == 0, write 'video_done' to kernel\n"));
        write(efd_l2k, &video_done, sizeof(video_done));
        return 0;
    } else if (blk_size < 0) {
        PRINT(("blk_size < 0, write 'abort' to kernel\n"));
        write(efd_l2k, &abort, sizeof(abort));
        return blk_size;
    } 

    PRINT(("in handle_block(%d). write to the kernel that new data is available\n", blk_size));

    new_data_available = PACK(NEW_DATA_AVAILABLE, blk_size);
    int retval = write(efd_l2k, &new_data_available, sizeof(new_data_available));
    if (retval != sizeof(new_data_available)) {
        fprintf(stderr, "bad retval on write. %d but expected %d\n", retval, sizeof(new_data_available));
        return -1;
    }

    PRINT(("wait until the kernel signals that it is ready for new data\n"));
    retval = read(efd_k2l, &kernel_state, sizeof(kernel_state));
    if (retval != sizeof(kernel_state)) {
        if (retval == -1 && errno == EINTR) {
            fprintf(stderr, "interrupted, exiting\n");
        } else {
            fprintf(stderr, "bad retval on read. %d but expected %d\n", retval, sizeof(kernel_state));
        }
        return -1;
    }

    if (kernel_state != READY_FOR_NEW_DATA) { // received EOF signal
        PRINT(("unexpected kernel_state. expected %d but got %llu, return 0\n", READY_FOR_NEW_DATA, kernel_state));
        return 0;
    }
    PRINT(("kernel ready for new data, continue...\n"));
    return blk_size;
}

// listen and accept connection from the player process
int socket_to_player() {
    struct sockaddr_un local, remote;
    
    unsigned int s = socket(AF_UNIX, SOCK_STREAM, 0);
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, "./drm_socket");
    unlink(local.sun_path);
    int len = strlen(local.sun_path) + sizeof(local.sun_family);
    bind(s, (struct sockaddr*) &local, len);

    // listen and accept the player process
    listen(s, 1);
    len = sizeof(struct sockaddr_un);
    return accept(s, (struct sockaddr*) &remote, &len);
}

int recv_block(int server, unsigned char* dest) {
    int numbytes, left_to_read = BLK_SIZE;
    unsigned char* curr = dest;

    while (left_to_read > 0) {
        numbytes = recv(server, curr, left_to_read , 0);
        if (numbytes > 0) {
            left_to_read -= numbytes;
            curr += numbytes;
        }
        else if (numbytes == 0) {
            break;
        }
        else {
            perror("recv");
            exit(1);
        }
    }

    return BLK_SIZE - left_to_read;
}

int run(unsigned char *key) {

    struct sigaction int_handler = {.sa_handler=handle_int};
    int r,s;
    sigaction(SIGINT,&int_handler,0);

    const int socktype = TCP ? SOCK_STREAM : SOCK_DGRAM;

    int server = socket_to_server(socktype);
    if (server < 0) {
        fprintf(stderr, NAME " creation failed, exiting\n");
        goto cleanups;
    }
    // send a request to the server and read the video metadata
    Metadata metadata;
    s = send(server, key, KEY_LEN + strlen(key + KEY_LEN), 0);
    r = recv(server, &metadata, sizeof(metadata), 0); 
    if (r != sizeof(metadata)) {
        fprintf(stderr, "reading metadata failed\n");
        goto cleanups;
    }

    PRINT(("read metadata from server: width = %d, height = %d, frames = %d, will forward to player\n", metadata.width, metadata.height, metadata.frames));

    // send the metadata to the player
    int player = socket_to_player();
    Metadata ack;
    if (player < 0) {
        fprintf(stderr, NAME " creation failed, exiting\n");
        goto cleanups;
    }
    send(player, &metadata, sizeof(metadata), 0);
    PRINT(("waiting for player metadata ack..."));

    recv(player, &ack, sizeof(ack), 0);
    if (!equal(metadata, ack)) {
        fprintf(stderr, "invalid metadata ack received.");
        goto cleanups;
    }
    close(player);

    // metadata negotiation done, setup kernel context
    PRINT(("metadata negotiation done, setup kernel context\n"));

    unsigned char* addr = setup_kernel_context(metadata);
    if (!addr) {
        goto cleanups;
    }

    bufs[0] = addr; 
    bufs[1] = addr + BLK_SIZE;

    int blk_size = -1;


    while (!interrupted) {
        static int res;
        PRINT(("waiting to recv a new block... "));
        blk_size = recv_block(server, bufs[blk_id]);
        res = handle_block(blk_size);
        if (res <= 0)
            break;
        blk_id = !blk_id;
    }

cleanups:
    if (addr) { 
        munmap(addr, BUFFER_SIZE);
    }
    if (player > 0) {
        close(player);
    }
    cleanup(0);
    PRINT(("done !\n"));

    return 0;
}

// ask the kernel to randomize a key and encrypt it using the server public key
int get_encrypted_key(unsigned char* key) {

	unsigned char opn_buf[256], ope_buf[4], opd_buf[256];
	char m[256];
	int i,j;
	read_buffer("opn.bin", opn_buf, 256);
	read_buffer("ope.bin", ope_buf, 4);
	read_buffer("opd.bin", opd_buf, 256);

	rsa_op *ope, *opn, *opd;
	rsa_op_init(&ope, ope_buf, 4,   0);
	rsa_op_init(&opn, opn_buf, 256, 0);
	rsa_op_init(&opd, opd_buf, 256, 0);

	gen_encrypted_key(opn, ope, key);
	return 0;
}

int min(int a, int b) {
    return (a < b) ? a : b;
}

void signal_handler(int signum, siginfo_t *sigi, void * context) {
	char msg[200] = {'\0'};
	strncpy(msg,__PRETTY_FUNCTION__,200);
	psiginfo(sigi, msg);
}

int main(int argc, char* argv[]) {
    int i,j;
    char video_id[VIDEO_ID_LEN] = {0};
	struct sigaction sigstruct;
	sigstruct.sa_sigaction = signal_handler;
	sigstruct.sa_flags = SA_SIGINFO;
	sigfillset(&sigstruct.sa_mask);
	sigaction(SIGINT, &sigstruct, NULL);	
	sigaction(SIGTERM, &sigstruct, NULL);	
	sigaction(SIGTERM, &sigstruct, NULL);	
    if (argc == 2) {
        memcpy(video_id, argv[1], min(VIDEO_ID_LEN, strlen(argv[1])));
    } else {
        fprintf(stderr, "no video_id was gived. will use good.mpg\n");
        memcpy(video_id, "good.mpg", min(VIDEO_ID_LEN, strlen("good.mpg")));
    }
    unsigned char key[KEY_LEN + VIDEO_ID_LEN] = {0}; 
    get_encrypted_key(key);
	memcpy(key + KEY_LEN, video_id, VIDEO_ID_LEN);
    return run(key);
}

