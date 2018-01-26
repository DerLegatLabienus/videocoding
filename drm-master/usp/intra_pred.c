#include "common.h"
#include "mbmodes.h"
#include "mode_pred.h"
#include "intra_pred.h"

//////////////////////////////////////////////////// INTRA_4X4 PREDICTION /////

#define p(x,y)  L_pixel(f,bx+(x),by+(y))
#define left(y) ref[3-(y)]
#define top(x)  ref[5+(x)]

//          0      1      2      3       4      5      6      7      8      9     10     11     12
// ref: [ l(3) | l(2) | l(1) | l(0) | l(-1) | t(0) | t(1) | t(2) | t(3) | t(4) | t(5) | t(6) | t(7) ]

// 0. the 4 upper neighbor pixels are repeated downwards
static inline void Intra_4x4_Vertical(frame *f, int *ref, int bx, int by) {
  int x,y;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x)
      p(x,y)=top(x);
}

// 1. the 4 left neighbor pixers are repeated to the right
static inline void Intra_4x4_Horizontal(frame *f, int *ref, int bx, int by) {
  int x,y;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x)
      p(x,y)=left(y);
}

// 2. the block is filled with the average of the 4 left and 4 upper neighbor pixels (8.2.1.2.3 in std)
static inline void Intra_4x4_DC(frame *f, mode_pred_info *mpi, int x, int y) {
  int i, sum=0, count=0;
  if(x>0 && ModePredInfo_Intra4x4PredMode(mpi,(x>>2)-1,y>>2)>=0)
    for(i=0; i<4; ++i, ++count)
      sum+=L_pixel(f,x-1,y+i);
  if(y>0 && ModePredInfo_Intra4x4PredMode(mpi,x>>2,(y>>2)-1)>=0)
    for(i=0; i<4; ++i, ++count)
      sum+=L_pixel(f,x+i,y-1);
  if(count==8) sum=(sum+4)>>3; else
  if(count==4) sum=(sum+2)>>2; else
               sum=128;
  sum=Clip(sum);
  for(i=0; i<4; ++i)
    memset(&L_pixel(f,x,y+i),sum,4);
}

// 3. pixels are repeated in a 45-degree angle from the upper right to the lower left corner
static inline void Intra_4x4_Diagonal_Down_Left(frame *f, int *ref, int bx, int by) {
  int x,y,i;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x) {
      if((x&y)==3) i=top(6)+3*top(7)+2;
      else         i=top(x+y)+2*top(x+y+1)+top(x+y+2)+2;
      p(x,y)=i>>2;
    }
}

// 4. pixels are repeated in a 45-degree angle from the upper left to the lower right corner
static inline void Intra_4x4_Diagonal_Down_Right(frame *f, int *ref, int bx, int by) {
  int x,y,i;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x) {
           if(x>y) i=top(x-y-2)+2*top(x-y-1)+top(x-y)+2;
      else if(x<y) i=left(y-x-2)+2*left(y-x-1)+left(y-x)+2;
      else         i=top(0)+2*top(-1)+left(0)+2;
      p(x,y)=i>>2;
    }
}

// 5. pixels are repeated in an approx. 30-degree angle from the upper edge to the lower
// edge, slightly drifting to the right
static inline void Intra_4x4_Vertical_Right(frame *f, int *ref, int bx, int by) {
  int x,y,i,zVR;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x) {
      zVR=2*x-y;
           if(zVR<-1) i=left(y-1)+2*left(y-2)+left(y-3)+2;
      else if(zVR<0)  i=left(0)+2*left(-1)+top(-1)+2;
      else if(zVR&1)  i=top(x-(y>>1)-2)+2*top(x-(y>>1)-1)+top(x-(y>>1))+2;
      else            i=2*top(x-(y>>1)-1)+2*top(x-(y>>1))+2;
      p(x,y)=i>>2;
    }
}

// 6. pixels are repeated in an approx. 30-degree angle from the left edge to the right
// edge, slightly drifting downwards
static inline void Intra_4x4_Horizontal_Down(frame *f, int *ref, int bx, int by) {
  int x,y,i,zHD;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x) {
      zHD=2*y-x;
           if(zHD<-1) i=top(x-1)+2*top(x-2)+top(x-3)+2;
      else if(zHD<0)  i=left(0)+2*left(-1)+top(0)+2;
      else if(zHD&1)  i=left(y-(x>>1)-2)+2*left(y-(x>>1)-1)+left(y-(x>>1))+2;
      else            i=2*left(y-(x>>1)-1)+2*left(y-(x>>1))+2;
      p(x,y)=i>>2;
    }
}

// 7. pixels are repeated in an approx. 30-degree angle from the upper edge to the lower
// edge, slightly drifting to the left
static inline void Intra_4x4_Vertical_Left(frame *f, int *ref, int bx, int by) {
  int x,y,i;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x) {
      if(y&1) i=top(x+(y>>1))+2*top(x+(y>>1)+1)+top(x+(y>>1)+2)+2;
      else    i=2*top(x+(y>>1))+2*top(x+(y>>1)+1)+2;
      p(x,y)=i>>2;
    }
}

// 8. pixels are repeated in an approx. 30-degree angle from the left edge to the rightr
// edge, slightly drifting upwards
static inline void Intra_4x4_Horizontal_Up(frame *f, int *ref, int bx, int by) {
  int x,y,i,zHU;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x) {
      zHU=x+2*y;
           if(zHU>5)  i=4*left(3);
      else if(zHU==5) i=left(2)+3*left(3)+2;
      else if(zHU&1)  i=left(y+(x>>1))+2*left(y+(x>>1)+1)+left(y+(x>>1)+2)+2;
      else            i=2*left(y+(x>>1))+2*left(y+(x>>1)+1)+2;
      p(x,y)=i>>2;
    }
}

////////////////////////////////

//#define p(x,y)  L_pixel(f,bx+(x),by+(y))
//#define left(y) ref[3-(y)]   -1 <= y <= 3
//#define top(x)  ref[5+(x)]    0 <= x <= 7

// Loads the surrounding pixels used for the prediction process into a temporary array.
// If some of the reference pixels are not available (because they are not yet decoded),
// the available edge pixels are repeated accordingly
void Intra_4x4_Dispatch(frame *f, mode_pred_info *mpi, int x, int y, int luma4x4BlkIdx) {
  int ref[13];
  int mode=ModePredInfo_Intra4x4PredMode(mpi,x>>2,y>>2);

  if(mode!=2) {
    int i;

    if(x>0) {
      for(i=0; i<4; ++i) 
          left(i) = L_pixel(f,x-1,y+i);
      if(y>0) 
          left(-1) = L_pixel(f,x-1,y-1);
    }

    if(y>0) {
      for(i=0; i<4; ++i) 
          top(i) = L_pixel(f,x+i,y-1);

      if((luma4x4BlkIdx & 3)==3 || luma4x4BlkIdx==13 || (luma4x4BlkIdx==5 && x>=f->Lwidth-4))
        for(i=4; i<8; ++i) 
            top(i) = top(3);
      else
        for(i=4; i<8; ++i) 
            top(i) = L_pixel(f,x+i,y-1);
    }
  }

  switch(mode) {
    case 0: Intra_4x4_Vertical(f,&ref[0],x,y); break;
    case 1: Intra_4x4_Horizontal(f,&ref[0],x,y); break;
    case 2: Intra_4x4_DC(f,mpi,x,y); break;
    case 3: Intra_4x4_Diagonal_Down_Left(f,&ref[0],x,y); break;
    case 4: Intra_4x4_Diagonal_Down_Right(f,&ref[0],x,y); break;
    case 5: Intra_4x4_Vertical_Right(f,&ref[0],x,y); break;
    case 6: Intra_4x4_Horizontal_Down(f,&ref[0],x,y); break;
    case 7: Intra_4x4_Vertical_Left(f,&ref[0],x,y); break;
    case 8: Intra_4x4_Horizontal_Up(f,&ref[0],x,y); break;
    default: printf("unsupported Intra4x4PredMode %d at %d,%d!\n",mode,x,y);
  } 
}

#undef p
#undef top
#undef left


////////////////////////////////////////////////// INTRA_16X16 PREDICTION /////

#define p(x,y)  L_pixel(f,bx+(x),by+(y))

static inline void Intra_16x16_Vertical(frame *f, int bx, int by) {
  int x,y;
  for(y=0; y<16; ++y)
    for(x=0; x<16; ++x)
      p(x,y)=p(x,-1);
}

static inline void Intra_16x16_Horizontal(frame *f, int bx, int by) {
  int x,y;
  for(y=0; y<16; ++y)
    for(x=0; x<16; ++x)
      p(x,y)=p(-1,y);
}

static inline void Intra_16x16_DC(frame *f, mode_pred_info *mpi, int bx, int by, int constrained_intra_pred) {
  int i, sum=0, count=0;
  i=get_mb_mode(mpi,(bx>>4)-1,by>>4);
  if(!((i==NA) || (IsInter(i) && constrained_intra_pred)))
    for(i=0; i<16; ++i, ++count)
      sum+=p(-1,i);
  i=get_mb_mode(mpi,bx>>4,(by>>4)-1);
  if(!((i==NA) || (IsInter(i) && constrained_intra_pred)))
    for(i=0; i<16; ++i, ++count)
      sum+=p(i,-1);
  if(count==32) sum=(sum+16)>>5; else
  if(count==16) sum=(sum+8)>>4;  else
                sum=128;
  sum=Clip(sum);
  for(i=0; i<16; ++i)
    memset(&p(0,i),sum,16);
}

static inline void Intra_16x16_Plane(frame *f, int bx, int by) {
  int a,b,c,H,V,x,y;
  for(x=0, H=0; x<8; ++x) H+=(x+1)*(p(8+x,-1)-p(6-x,-1));
  for(y=0, V=0; y<8; ++y) V+=(y+1)*(p(-1,8+y)-p(-1,6-y));
  a=16*(p(-1,15)+p(15,-1));
  b=(5*H+32)>>6; c=(5*V+32)>>6;
  for(y=0; y<16; ++y) for(x=0; x<16; ++x)
    p(x,y)=Clip((a+b*(x-7)+c*(y-7)+16)>>5);  
}

void Intra_16x16_Dispatch(frame *f, mode_pred_info *mpi, int mode, int x, int y, int constrained_intra_pred) {
  //fprintf(stderr, "Intra_16x16_Dispatch: mode %d, x %d, y %d\n", mode, x, y);
  switch(mode) {
    case 0: Intra_16x16_Vertical(f,x,y); break;
    case 1: Intra_16x16_Horizontal(f,x,y); break;
    case 2: Intra_16x16_DC(f,mpi,x,y,constrained_intra_pred); break;
    case 3: Intra_16x16_Plane(f,x,y); break;
    default: printf("unsupported Intra16x16PredMode %d at %d,%d!\n",mode,x,y);
  }
}

#undef p

///////////////////////////////////////////////// INTRA_CHROMA PREDICTION /////

#define r(x,y)  Cr_pixel(f,bx+(x),by+(y))
#define b(x,y)  Cb_pixel(f,bx+(x),by+(y))

#define ICDCsumL(chan,offs) chan(-1,offs)+chan(-1,offs+1)+chan(-1,offs+2)+chan(-1,offs+3)
#define ICDCsumT(chan,offs) chan(offs,-1)+chan(offs+1,-1)+chan(offs+2,-1)+chan(offs+3,-1)
#define ICDCfill(offx,offy,valr,valb) \
  do { \
    int sy,vr=valr,vb=valb; \
    for(sy=0; sy<4; ++sy) { \
      memset(&r(offx,offy+sy),vr,4); \
      memset(&b(offx,offy+sy),vb,4); \
    } \
  } while(0)

static inline void Intra_Chroma_DC(frame *f, mode_pred_info *mpi, int bx, int by, int constrained_intra_pred) {
  int i;
  int left=1, top=1;
  int l0r=512,l0b=512,l4r=512,l4b=512;
  int t0r=512,t0b=512,t4r=512,t4b=512;

  i=get_mb_mode(mpi,(bx>>3)-1,by>>3);
  if((i==NA) || (IsInter(i) && constrained_intra_pred)) left=0;
  if(left) {
    l0r=ICDCsumL(r,0); l0b=ICDCsumL(b,0);
    l4r=ICDCsumL(r,4); l4b=ICDCsumL(b,4);
  }

  i=get_mb_mode(mpi,bx>>3,(by>>3)-1);
  if((i==NA) || (IsInter(i) && constrained_intra_pred)) top=0;
  if(top) {
    t0r=ICDCsumT(r,0); t0b=ICDCsumT(b,0);
    t4r=ICDCsumT(r,4); t4b=ICDCsumT(b,4);
  }

  if(top) {
    if(left)    ICDCfill(0,0,(l0r+t0r+4)>>3,(l0b+t0b+4)>>3);
        else    ICDCfill(0,0,(t0r+2)>>2,(t0b+2)>>2);
  } else {
    if(left)    ICDCfill(0,0,(l0r+2)>>2,(l0b+2)>>2);
        else    ICDCfill(0,0,128,128);
  }

       if(top)  ICDCfill(4,0,(t4r+2)>>2,(t4b+2)>>2);
  else if(left) ICDCfill(4,0,(l0r+2)>>2,(l0b+2)>>2);
  else          ICDCfill(4,0,128,128);

       if(left) ICDCfill(0,4,(l4r+2)>>2,(l4b+2)>>2);
  else if(top)  ICDCfill(0,4,(t0r+2)>>2,(t0b+2)>>2);
  else          ICDCfill(0,4,128,128);

  if(top) {
    if(left)    ICDCfill(4,4,(l4r+t4r+4)>>3,(l4b+t4b+4)>>3);
        else    ICDCfill(4,4,(t4r+2)>>2,(t4b+2)>>2);
  } else {
    if(left)    ICDCfill(4,4,(l4r+2)>>2,(l4b+2)>>2);
        else    ICDCfill(4,4,128,128);
  }
}

void Intra_Chroma_Horizontal(frame *f, int bx, int by) {
  int x,y;
  for(y=0; y<8; ++y)
    for(x=0; x<8; ++x) {
      r(x,y)=r(-1,y);
      b(x,y)=b(-1,y);
    }
}

void Intra_Chroma_Vertical(frame *f, int bx, int by) {
  int x,y;
  for(y=0; y<8; ++y)
    for(x=0; x<8; ++x) {
      r(x,y)=r(x,-1);
      b(x,y)=b(x,-1);
    }
}

void Intra_Chroma_Plane(frame *f, int bx, int by) {
  int A,B,C,H,V,x,y;
  // Intra_Chroma_Plane prediction for Cr channel
  for(x=0, H=0; x<4; ++x) H+=(x+1)*(r(4+x,-1)-r(2-x,-1));
  for(y=0, V=0; y<4; ++y) V+=(y+1)*(r(-1,4+y)-r(-1,2-y));
  A=16*(r(-1,7)+r(7,-1));
  B=(17*H+16)>>5; C=(17*V+16)>>5;
  for(y=0; y<8; ++y) for(x=0; x<8; ++x)
    r(x,y)=Clip((A+B*(x-3)+C*(y-3)+16)>>5);
  // Intra_Chroma_Plane prediction for Cr channel
  for(x=0, H=0; x<4; ++x) H+=(x+1)*(b(4+x,-1)-b(2-x,-1));
  for(y=0, V=0; y<4; ++y) V+=(y+1)*(b(-1,4+y)-b(-1,2-y));
  A=16*(b(-1,7)+b(7,-1));
  B=(17*H+16)>>5; C=(17*V+16)>>5;
  for(y=0; y<8; ++y) for(x=0; x<8; ++x)
    b(x,y)=Clip((A+B*(x-3)+C*(y-3)+16)>>5);
}

void Intra_Chroma_Dispatch(frame *f, mode_pred_info *mpi, int mode, int x, int y, int constrained_intra_pred) {
  switch(mode) {
    case 0: Intra_Chroma_DC(f,mpi,x,y,constrained_intra_pred); break;
    case 1: Intra_Chroma_Horizontal(f,x,y); break;
    case 2: Intra_Chroma_Vertical(f,x,y); break;
    case 3: Intra_Chroma_Plane(f,x,y); break;
    default: fprintf(stdout, "unsupported IntraChromaPredMode %d at %d,%d!\n",mode,x<<1,y<<1);
  }
}

#undef r
#undef b
