#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <getopt.h>
#include <SDL/SDL.h>
#include "h264.h"
#include "perf.h"
#include "rc4.h"
#include "listener.h"
#include "common.h"
#include "queue.h"


#define BitsPerPixel 16
#define VERBOSE 1

extern rc4_state my_rc4_state;
extern struct queue_root* root;

void help(char *arg) {
  printf(
    "Usage: %s [<options>] <streamfile.264>\n"
    "<options> are:\n"
    "  -h, -help        Show this help\n"
    "  -z, -zoom <n>    Scale image by n\n"
    "  -r, -fps <n>     Set maximum frame rate to n fps\n"
    "      -fps 0       Wait for keypress after each frame\n"
    "  -f, -frames <n>  Set maximum number of decoded frames\n"
    "  -l, -loop        Enable looping\n"
    "  -b, -bench       Benchmark mode\n"
    "  -p, -perf        Enable performance counting\n"
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
  perf_enter("rendering");
  
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

/*

void do_bench(int maxframe) {
  int frames=0;
  struct timeval start,end;
  double tdiff;
  
  printf("Benchmarking ...\n");
  gettimeofday(&start,NULL);
  for(frames=0; h264_decode_frame(1); ++frames) {
    if(maxframe && frames>=maxframe) break;
    putchar('\r');
    fflush(stdout);
  }
  gettimeofday(&end,NULL);
  tdiff=end.tv_sec-start.tv_sec+0.000001*(end.tv_usec-start.tv_usec);
  printf("%d frames decoded in %.1f seconds\n= %.1f ms/frame = %.1f fps\n",
         frames,tdiff,1000.0*tdiff/frames,frames/tdiff);
  h264_close();
  perf_summarize();
  exit(0);
}
*/

extern int decode_slice_data_ctr;

int main(int argc, char *argv[]) {
  SDL_Surface *screen;
  SDL_Event ev;
  int width,height,zoom=1,loop=0,delay=40,bench=0,perf=0,maxframe=0;
  int pause=0,step=0,have_event;
  int info;
  int next_dts;
  char *filename=NULL;

  ///// Command Line Parameter Parsing ////////////////////////////////////////

  const static struct option options[]={
    {"help",   0, NULL, 'h'},
    {"loop",   0, NULL, 'l'},
    {"bench",  0, NULL, 'b'},
    {"perf",   0, NULL, 'p'},
    {"zoom",   1, NULL, 'z'},
    {"fps",    1, NULL, 'r'},
    {"frames", 1, NULL, 'f'},
    {"scale",  1, NULL, 's'},
    {"xy",     1, NULL, 'x'},
    {"rc4-key",1, NULL, 'k'},
    {0, 0, 0, 0}
  };

  //unsigned char key[] = "19fa33366002178d1c2f4faba8c78dfb83c757b7dc3a1c435a935605ec3cf8b3";
  unsigned char rc4_key[256] = {0};
  int arglen = 0;

  for(;;) {
    int res,longidx;
    res=getopt_long_only(argc,argv,"hlbpz:r:f:sxk:",options,&longidx);
    if(res==-1) break;   
    switch(res) {
      case 'h':
        help(argv[0]);
        return 0;
      case 'l':
        loop^=1;
        break;
      case 'b':
        bench^=1;
        break;
      case 'p':
        perf^=1;
        break;
      case 'z':
        zoom=atoi(optarg);
        if(zoom<1 || zoom>10) {
          fprintf(stderr,"%s: invalid zoom value `%s'\n",argv[0],optarg);
          return 1;
        }
        break;
      case 'r':
        delay=atoi(optarg);
        if(delay<0 || delay>255) {
          fprintf(stderr,"%s: invalid fps value `%s'\n",argv[0],optarg);
          return 1;
        }
        if(delay) delay=1000/delay;
        break;
      case 'f':
        maxframe=atoi(optarg);
        if(delay<0) {
          fprintf(stderr,"%s: invalid frame count value `%s'\n",argv[0],optarg);
          return 1;
        }
        break;
      case 's':
        fprintf(stderr, "%s: 's' unsupported yet\n", argv[0]);
        break;
      case 'x':
        fprintf(stderr, "%s: 'x' unsupported yet\n", argv[0]);
        break;
      case 'k':
        arglen = strlen(optarg);
        if (arglen > 256) arglen = 256;
        memcpy(rc4_key,optarg, arglen);
        fprintf(stderr, "set key to %s\n", rc4_key);
        break;
      case '?':
        break;
      case ':':
      default:
        return 1;
    }
  }

  if(optind>=argc) {
    fprintf(stderr,"%s: no filename specified\n",argv[0]);
    return 1;
  }
  filename=argv[optind];

  ///// Initialization ////////////////////////////////////////////////////////

  root = alloc_queue_root();
  if (setup_server() < 0) {
      fprintf(stderr, "couldn't setup server, exiting\n");
      exit(1);
  }
          
  pthread_t listener_thread_id;
  pthread_create(&listener_thread_id, NULL, listener_thread, filename);

  info=h264_open(filename);
  if(!info) return 1;
  width=H264_WIDTH(info); // 352
  height=H264_HEIGHT(info); // 240
  if (!width || !height) {
      fprintf(stderr, "could not read height/width, exiting\n");
      exit(1);
  }

  if(perf) perf_enable();
  if(bench) do_bench(maxframe);

  printf("H.264 stream: %dx%d, zooming to %dx%d\n",
         width,height,width*zoom,height*zoom);

  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) {
    fprintf(stderr,"%s: Error in SDL_Init: %s\n",argv[0],SDL_GetError());
    return 1;
  }
  atexit(SDL_Quit);
  SDL_WM_SetCaption(filename,"KeyJ's H.264 Viewer");


  screen=SDL_SetVideoMode(width*zoom,height*zoom,BitsPerPixel,SDL_HWSURFACE);
  if(!screen) {
    fprintf(stderr,"%s: Error in SDL_SetVideoMode: %s\n",argv[0],SDL_GetError());
    return 1;
  }


  rc4_init(&my_rc4_state, rc4_key, 256);
  printf("init key to %s\n", rc4_key);

  ///// Main Loop /////////////////////////////////////////////////////////////

  printf("Hint: ");
  if(delay) printf("[Space] toggles pause; ");
  printf("[Enter] advances to next frame\n");
  pause=(!delay);
  step=1;

  next_dts=SDL_GetTicks();
  for(;;) {
    if((!pause) || step) {
      printf("\r");
      frame *f = h264_decode_frame(VERBOSE);

      fflush(stdout);
      if(maxframe && h264_frame_no()>=maxframe)
        break;
      if(f)
        showframe(screen,f,zoom);
      else if(loop) {
        h264_rewind();
        f=h264_decode_frame(1);
        fflush(stdout);
        if(!f) return 1;
        showframe(screen,f,zoom);
      } else
        break;
      next_dts+=delay;
      if(step) step=0;
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

  printf("\nDecoding done.\n");
  h264_close();
  perf_summarize();
  printf("decode_slice_data_ctr = %d\n", decode_slice_data_ctr);
  return 0;
}
