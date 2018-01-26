#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <SDL/SDL.h>
#include <signal.h>

#include "../tpvm/h264.h"
#include "../tpvm/rc4.h"

#include "common.h"

#undef USE_X86_ASM

#define BitsPerPixel 16

int efd_k2v, efd_v2k;

void help(char *arg) {
    printf(
            "Usage: %s [<options>] <streamfile.264>\n"
            "<options> are:\n"
            "  -h, -help      Show this help\n"
            "  -z, -zoom <n>  Scale image by n\n"
            "  -r, -fps <n>   Set maximum frame rate to n fps\n"
            "      -fps 0     Wait for keypress after each frame\n"
            "  -m, -max <n>   Set maximum number of decoded frames\n"
            ,arg);
}

static inline int clamp_and_scale(int i) {
#ifdef USE_X86_ASM
    asm(" \
            sarl $20,  %%eax; \
            cdq;              \
            notl %%edx;       \
            andl %%edx,%%eax; \
            subl $256, %%eax; \
            cdq;              \
            notl %%edx;       \
            orl  %%edx,%%eax; \
            andl $255, %%eax; \
            ":"=a"(i):"a"(i):"%edx");
#else
    i>>=20;
    if(i<0)i=0; if(i>255)i=255;
#endif
    return i;
}

void showframe(SDL_Surface *screen, frame *f, int zoom) {
    int i,x,y,Y,Cb=0,Cr=0,R,G,B;
    unsigned char *pY,*pCb,*pCr;
    unsigned char *line;

#if (BitsPerPixel==32)
#define DEST_TYPE unsigned int
    DEST_TYPE cache;
#define BytesPerPixel 4
#endif

#if (BitsPerPixel==24)
#define DEST_TYPE unsigned char
#define BytesPerPixel 3
#endif

#if (BitsPerPixel==15) || (BitsPerPixel==16)
#define DEST_TYPE unsigned short
    DEST_TYPE cache;
#define BytesPerPixel 2
#endif

    DEST_TYPE *dest;

    SDL_LockSurface(screen);
    line=screen->pixels;
    for(y=0; y<f->Lheight; ++y) {
        pY=&L_pixel(f,0,y);
        pCb=&Cb_pixel(f,0,y>>1);
        pCr=&Cr_pixel(f,0,y>>1);
        dest=(DEST_TYPE *)line;
        for(x=0; x<f->Lwidth; ++x) {
            Y=*pY++;
            if(!(x&1)) {
                Cb=*pCb++;
                Cr=*pCr++;
            }

            { int Y1,Pb,Pr;
                Y1= (Y-16) *1225732;
                Pb=(Cb-128)*1170;
                Pr=(Cr-128)*1170;
                R=clamp_and_scale(Y1        +1436*Pr);
                G=clamp_and_scale(Y1- 352*Pb -731*Pr);
                B=clamp_and_scale(Y1+1815*Pb);
            }

#if (BitsPerPixel==32)
            cache=(R<<16)|(G<<8)|B;
            for(i=zoom; i; --i)
                *dest++=cache;
#endif

#if (BitsPerPixel==24)
            for(i=zoom; i; --i) {
                *dest++=B;
                *dest++=G;
                *dest++=R;
            }
#endif

#if (BitsPerPixel==16)
            cache=((R>>3)<<11)|((G>>2)<<5)|(B>>3);
            for(i=zoom; i; --i)
                *dest++=cache;
#endif

#if (BitsPerPixel==15)
            cache=((R>>3)<<10)|((G>>3)<<5)|(B>>3);
            for(i=zoom; i; --i)
                *dest++=cache;
#endif

        }
        dest=(DEST_TYPE *)line;
        line+=screen->pitch;
        for(i=zoom; i>1; --i) {
            memcpy(line,dest,f->Lwidth*zoom*BytesPerPixel);
            line+=screen->pitch;      
        }
    }
    SDL_UnlockSurface(screen);
    SDL_UpdateRect(screen,0,0,0,0);
}

int read_dimensions_from_module(int* height, int* width) {
    FILE* proc_file = fopen("/proc/tpvm_reso", "r");
    if (!proc_file)
        return -1;
    fscanf(proc_file, "%d %d", height, width);
    fclose(proc_file);
    return 0;
}

int read_metadata_from_fetcher(int socket, Metadata* metadata) {
    if (socket <= 0) {
        fprintf(stderr, "fetcher socket = %d, exiting", socket);
        return -1;
    }
    return recv(socket, metadata, sizeof(*metadata), 0) == sizeof(*metadata) ? 0 : -1;
}

int send_metadata_to_fetcher(int socket, Metadata* metadata) {
    if (socket <= 0) {
        fprintf(stderr, "fetcher socket = %d, exiting", socket);
        return -1;
    }
    return send(socket, metadata, sizeof(*metadata), 0) == sizeof(*metadata) ? 0 : -1;
}

int socket_to_fetcher() {

    int s;
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    const char SOCK_PATH[] = "./drm_socket";
    strcpy(remote.sun_path, SOCK_PATH);
    int len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        return -1;
    }

    return s;
}

int main(int argc, char *argv[]) {
    int ret_value = EXIT_FAILURE;
    SDL_Surface *screen;
    SDL_Event ev;
    int width,height,zoom=1,loop=0,delay=40,max_frame_num=0;
    int pause=0,step=0,have_event;
    int next_dts;

    ///// Command Line Parameter Parsing ////////////////////////////////////////

    const static struct option options[]={
        {"help",   0, NULL, 'h'},
        {"zoom",   1, NULL, 'z'},
        {"fps",    1, NULL, 'r'},
        {"max",    1, NULL, 'm'},
        {0, 0, 0, 0}
    };

    for(;;) {
        int res,longidx;
        res=getopt_long_only(argc,argv,"hz:r:m:",options,&longidx);
        if(res==-1) break;   
        switch(res) {
            case 'h':
                help(argv[0]);
                return 0;
            case 'z':
                zoom=atoi(optarg);
                if(zoom<1 || zoom>10) {
                    fprintf(stderr,"%s: invalid zoom value `%s'\n",argv[0],optarg);
                    goto cleanups;
                }
                break;
            case 'r':
                delay=atoi(optarg);
                if(delay<0 || delay>255) {
                    fprintf(stderr,"%s: invalid fps value `%s'\n",argv[0],optarg);
                    goto cleanups;
                }
                if(delay) delay=1000/delay;
                break;
            case 'm':
                max_frame_num=atoi(optarg);
                fprintf(stderr, "max_frame_num = %d\n", max_frame_num);
                if(delay<0) {
                    fprintf(stderr,"%s: invalid frame count value `%s'\n",argv[0],optarg);
                    goto cleanups;
                }
                break;
            case '?':
                break;
            case ':':
            default:
                fprintf(stderr, "%s: invalid argument\n", argv[0]);
                goto cleanups;
        }
    }

    // register interrupt handler
    /* struct sigaction int_handler = {.sa_handler=handle_int}; */
    /* sigaction(SIGINT, &int_handler,0); */

    ///// Initialization ////////////////////////////////////////////////////////
    int fetcher = socket_to_fetcher();
    if (fetcher <= 0) {
        fprintf(stderr, "Could not open socket to fetcher\n");
        goto cleanups;
    }
    Metadata metadata;
    if (read_metadata_from_fetcher(fetcher, &metadata) < 0) {
        fprintf(stderr, "Failed reading metadata from fetcher! Exiting...\n");
        goto cleanups;
    }

    if (!is_valid(metadata)) {
        fprintf(stderr, "Read invalid metadata: width = %d, height = %d. exiting...\n", metadata.width, metadata.height);
        goto cleanups;
    }

    if (send_metadata_to_fetcher(fetcher, &metadata) < 0) {
        fprintf(stderr, "Failed sending metadata to fetcher! Exiting...\n");
        goto cleanups;
    }

    width = metadata.width;
    height = metadata.height;
    max_frame_num = metadata.frames;

    efd_k2v = eventfd(0,0);
    efd_v2k = eventfd(0,0);
    if (write_efds(efd_k2v, efd_v2k, "/proc/tpvm_viewer_info") < 0) {
        goto cleanups;
    }

    // open the kernel-module device
    const int dev_fd = open(dev_path, O_RDWR);
    if (dev_fd <= 0) {
        fprintf(stderr, "failed opening device %s! exiting\n", dev_path);
        goto cleanups;
    }

    // after the device is opened, we can mmap the buffer 

    const int Ltotal = height*width, Ctotal = Ltotal>>2,
              frame_size = sizeof(frame) + Ltotal + 2*Ctotal;

    unsigned char* addr = mmap(NULL, BLKS * BLK_SIZE + 2 * frame_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0); 
    if (addr == MAP_FAILED) {
        fprintf(stderr, "mmap failed, exiting\n");
        goto cleanups;
    }

    frame* frames[2] = {(frame*)(addr + BLKS * BLK_SIZE), (frame*)(addr + BLKS * BLK_SIZE + frame_size)};

    // init SDL
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) {
        fprintf(stderr,"%s: Error in SDL_Init: %s\n",argv[0],SDL_GetError());
        goto cleanups;
    }
    atexit(SDL_Quit);

    screen=SDL_SetVideoMode(width*zoom, height*zoom, BitsPerPixel, SDL_HWSURFACE);
    if(!screen) {
        fprintf(stderr,"%s: Error in SDL_SetVideoMode: %s\n",argv[0],SDL_GetError());
        goto cleanups;
    }

    // we're ready, signal kernel to start the decoder thread
    int ret = ioctl(dev_fd, KEYJ_IOCRENDERER, 0);
    if (ret < 0) {
        fprintf(stderr, "failed invocking ioctl, exiting\n");
        goto cleanups;
    }

    ///// Main Loop /////////////////////////////////////////////////////////////

    pause=(!delay);
    step=1;
    next_dts=SDL_GetTicks();

    static const uint64_t ready_for_new_data = READY_FOR_NEW_DATA;
    uint64_t is_new_data_available = NA;
    int buf_id    = 0;
    int frame_num = 0;
    while(1) {
        if((!pause) || step) {

            int retval = read(efd_k2v, &is_new_data_available, sizeof(uint64_t));
            if (retval != sizeof(uint64_t)) {
                fprintf(stderr, "retval = %d\n, expected %d\n", retval, sizeof(uint64_t));
                goto cleanups;
            }

            if (NEW_DATA_AVAILABLE != is_new_data_available) { // eof signal, or abort
                if (VIDEO_DONE == is_new_data_available) {
                    break;
                } else {
                    fprintf(stderr, "expected 'is_new_data_available' to be %d but its %llu, breaking\n", NEW_DATA_AVAILABLE, is_new_data_available);
                    goto cleanups;
                }
            }

            unsigned char* pc = (unsigned char*)frames[buf_id] + sizeof(frame);
            frames[buf_id]->L    = pc;
            frames[buf_id]->C[0] = pc + Ltotal;
            frames[buf_id]->C[1] = pc + Ltotal + Ctotal;

            showframe(screen, frames[buf_id], zoom);

            frame_num++;
            if (max_frame_num && frame_num == max_frame_num - 5) {
                PRINT(("max_frame_num && frame_num == %d, breaking\n", frame_num));
                break;
            }
            next_dts+=delay;
            if(step) step=0;

            buf_id = !buf_id;

            // now notify the module that we have displayed the frame
            write(efd_v2k, &ready_for_new_data, sizeof(ready_for_new_data));
        }

        if(!pause) {
            int wait=next_dts-SDL_GetTicks();
            if(wait>0) SDL_Delay(wait);
        }

        if(pause) {
            if(!SDL_WaitEvent(&ev))
                break;
            have_event=1;
        } else
            have_event=SDL_PollEvent(&ev);

        if(have_event) {
            if(ev.type==SDL_QUIT || (ev.type==SDL_KEYDOWN &&
                        (ev.key.keysym.sym==SDLK_ESCAPE || ev.key.keysym.sym==SDLK_q)))
                break;
            if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_RETURN)
                step=1;
            if(delay && ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_SPACE)
                pause^=1;
        }
    }

    ret_value = EXIT_SUCCESS;

    // cleanups
cleanups:
    if (fetcher > 0) {
        close(fetcher);
    }
    if (addr) {
        munmap(addr, 2 * frame_size);
    }
    if (dev_fd > 0) {
        close(dev_fd);
    }

    return ret_value;
}

