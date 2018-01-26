#include "common.h"
#include "params.h"
#include "main.h"

#include <linux/slab.h>

#define FLAGS GFP_KERNEL

// allocate the whole required space as contiguous chunk
frame *alloc_frame(int width, int height) {
    const int wh = width*height, 
              frame_size = sizeof(frame) + wh + (wh>>1); // each chroma array has size wh/4

    unsigned char* addr = kmalloc(frame_size, FLAGS);

    frame *f = (frame*)addr;
    f->L     = addr + sizeof(frame);
    f->C[0]  = addr + sizeof(frame) + wh;
    f->C[1]  = addr + sizeof(frame) + wh  + wh/4;

    f->Lheight = height;
    f->Cheight = height>>1;
    f->Lwidth  = f->Lpitch = width;
    f->Cwidth  = f->Cpitch = width>>1;

    return f;
}

void free_frame(frame *f) {
    if (f)
        kfree(f);
}

