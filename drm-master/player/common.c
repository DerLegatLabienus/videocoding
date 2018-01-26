#include "common.h"
#include "../tpvm/protocol.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int write_efds(int efd1, int efd2, const char* path)
{
    if (efd1 == -1 || efd2 == -1){
        fprintf(stderr, "\nUnable to create eventfd! Exiting...\n");
        return -1;
    }                                                                                                                           

    /* let kernel module know our (pid, efds) to be able to send us signals */
    const int ep_fd = open(path, O_WRONLY);
    if(ep_fd < 0) {
        fprintf(stderr, "Failed opening %s (%s)! Exiting...\n", path, strerror(errno));
        return -1;
    }
    char buf[20];
    snprintf(buf, 20, "%d %d %d", getpid(), efd1, efd2);
    if (write(ep_fd, buf, strlen(buf) + 1) < 0) {
        fprintf(stderr, "write to %s failed (%s)!\n", path, strerror(errno));
        close(ep_fd);
        return -1;
    }
    close(ep_fd);
    return 0;
}

int is_valid(const Metadata metadata) {
    return (metadata.width > 0 && metadata.height > 0 && metadata.frames > 0);
}

int equal(const Metadata a, const Metadata b) {
    return (a.width == b.width && a.height == b.height && a.frames == b.frames);
}

