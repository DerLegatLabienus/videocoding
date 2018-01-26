#include "common.h"
#include "input.h"

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

int in_file_err=0;
struct file* filp=NULL;
static unsigned long long offset=0;

extern int input_size;
extern int input_remain;
extern int ring_pos;
extern unsigned char ring_buf[RING_BUF_SIZE];

/*
 * int input_open(char *filename) {
 *   if(input_fd) {
 *     fprintf(stderr,"input_open: file already opened\n");
 *     return 0;
 *   }
 *   input_fd=fopen(filename,"rb");
 *   if(!input_fd) {
 *     perror("input_open: cannot open file");
 *     return 0;
 *   }
 *   fseek(input_fd,0,SEEK_END);
 *   input_size=ftell(input_fd);
 *   input_rewind();
 *   return input_size;
 * }      
 */

struct file* file_open(const unsigned char* path) {
    const int FLAGS = 0, RIGHTS = 0;
    struct file* _filp = NULL;
    const mm_segment_t oldfs = get_fs();

    set_fs(get_ds());
    _filp = filp_open(path, FLAGS, RIGHTS);
    set_fs(oldfs);

    if(IS_ERR(_filp)) {
        in_file_err = PTR_ERR(_filp);
        return NULL;
    }
    return _filp;
}


int input_open(const unsigned char* path) {
    const int size = 131284; // TODO: this is the size of Aladin.264. need to actually calculate the size of file
    filp = file_open(path);

    if (NULL == filp) {
        return -1;
    }
    input_rewind();
    return size;
}

/* int input_read(unsigned char *dest, int size) { */
/*     int count=fread(dest,1,size,input_fd); */
/*     input_remain+=count; */
/*     return count; */
/* } */

int input_read(unsigned char* dest, int size) {
    int ret;
    const mm_segment_t oldfs = get_fs();

    set_fs(get_ds());
    ret = vfs_read(filp, dest, size, &offset);
    set_fs(oldfs);
    input_remain += ret;

    return ret;
}   

void input_rewind() {
    if(!filp) {
        printk(KERN_ERR "input_rewind called, but no file opened!\n");
        return;
    }
    offset=0;
    input_remain=0;
    input_read(ring_buf, RING_BUF_SIZE);
    ring_pos=0;
}

void input_close() {
    if(!filp) return;
    filp_close(filp, NULL);
    input_size=0;
    filp=NULL;
}
