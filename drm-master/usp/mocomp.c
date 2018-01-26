#include "common.h"
#include "mode_pred.h"
#include "mocomp.h"

typedef struct L_MC_temp_block  {
  int p[9][9];
} L_MC_temp_block;

typedef struct C_MC_temp_block {
  int p[3][3];
} C_MC_temp_block;


static inline L_MC_temp_block GetLMCTempBlock(frame *ref, int org_x, int org_y) {
  L_MC_temp_block b;
  int x,y,sx,sy;
//printf("L_MC_temp_block from %d,%d:\n",org_x,org_y);
  for(y=0; y<9; ++y) {
    sy=org_y+y;
    if(sy<0) sy=0;
    if(sy>=ref->Lheight) sy=ref->Lheight-1;
    for(x=0; x<9; ++x) {
      sx=org_x+x;
      if(sx<0)            b.p[y][x]=L_pixel(ref,0,sy); else
      if(sx>=ref->Lwidth) b.p[y][x]=L_pixel(ref,ref->Lwidth-1,sy);
                     else b.p[y][x]=L_pixel(ref,sx,sy);
//printf("%4d",b.p[y][x]);
    }
//printf("\n");
  }
  return b;
}


#define Filter(E,F,G,H,I,J) Clip1(((E)-5*(F)+20*(G)+20*(H)-5*(I)+(J)+16)>>5)
static inline int Clip1(int i) {
  if(i<0) return 0; else if(i>255) return 255; else return i;
}

#define iffrac(x,y) if(frac==y*4+x)
#define Mix(a,b) (((a)+(b)+1)>>1)

static inline int L_MC_get_sub(int *data, int frac) {
#define p(x,y) data[(y)*9+(x)]
  int b,cc,dd,ee,ff,h,j,m,s;
  iffrac(0,0) return p(0,0);
  b=Filter(p(-2,0),p(-1,0),p(0,0),p(1,0),p(2,0),p(3,0));
  iffrac(1,0) return Mix(p(0,0),b);
  iffrac(2,0) return b;
  iffrac(3,0) return Mix(b,p(1,0));
  h=Filter(p(0,-2),p(0,-1),p(0,0),p(0,1),p(0,2),p(0,3));
  iffrac(0,1) return Mix(p(0,0),h);
  iffrac(0,2) return h;
  iffrac(0,3) return Mix(h,p(0,1));
  iffrac(1,1) return Mix(b,h);
  m=Filter(p(1,-2),p(1,-1),p(1,0),p(1,1),p(1,2),p(1,3));
  iffrac(3,1) return Mix(b,m);
  s=Filter(p(-2,1),p(-1,1),p(0,1),p(1,1),p(2,1),p(3,1));
  iffrac(1,3) return Mix(h,s);
  iffrac(3,3) return Mix(s,m);
  cc=Filter(p(-2,-2),p(-2,-1),p(-2,0),p(-2,1),p(-2,2),p(-2,3));
  dd=Filter(p(-1,-2),p(-1,-1),p(-1,0),p(-1,1),p(-1,2),p(-1,3));
  ee=Filter(p(2,-2),p(2,-1),p(2,0),p(2,1),p(2,2),p(2,3));
  ff=Filter(p(3,-2),p(3,-1),p(3,0),p(3,1),p(3,2),p(3,3));
  j=Filter(cc,dd,h,m,ee,ff);
  iffrac(2,2) return j;
  iffrac(2,1) return Mix(b,j);
  iffrac(1,2) return Mix(h,j);
  iffrac(2,3) return Mix(j,s);
  iffrac(3,2) return Mix(j,m);
  return 128;  // when we arrive here, something's going seriosly wrong ...
#undef p
}


static inline C_MC_temp_block GetCMCTempBlock(frame *ref, int iCbCr, int org_x, int org_y) {
  C_MC_temp_block b;
  int x,y,sx,sy;
//printf("C_MC_temp_block (c#%d) from %d,%d:\n",iCbCr,org_x,org_y);
  for(y=0; y<3; ++y) {
    sy=org_y+y;
    if(sy<0) sy=0;
    if(sy>=ref->Cheight) sy=ref->Cheight-1;
    for(x=0; x<3; ++x) {
      sx=org_x+x;
      if(sx<0)            b.p[y][x]=C_pixel(ref,iCbCr,0,sy); else
      if(sx>=ref->Cwidth) b.p[y][x]=C_pixel(ref,iCbCr,ref->Cwidth-1,sy);
                     else b.p[y][x]=C_pixel(ref,iCbCr,sx,sy);
//printf("|%3d%4d",sx,b.p[y][x]);
    }
//printf("\n");
  }
  return b;
}


void MotionCompensateTB(frame *this, frame *ref,
                        int org_x, int org_y,
                        int mvx, int mvy) {
  int x,y,iCbCr;
  L_MC_temp_block b=GetLMCTempBlock(ref,org_x+(mvx>>2)-2,org_y+(mvy>>2)-2);
  int frac=(mvy&3)*4+(mvx&3);
//printf("org=%d,%d mv=%d,%d frac=%d\n",org_x,org_y,mvx,mvy,frac);
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x)
      L_pixel(this,x+org_x,y+org_y)=L_MC_get_sub(&(b.p[y+2][x+2]),frac);

  org_x>>=1; org_y>>=1;
  for(iCbCr=0; iCbCr<2; ++iCbCr) {
    C_MC_temp_block b=GetCMCTempBlock(ref,iCbCr,
                      org_x+(mvx>>3),org_y+(mvy>>3));
    int xFrac=(mvx&7), yFrac=(mvy&7);
    for(y=0; y<2; ++y)
      for(x=0; x<2; ++x)
        C_pixel(this,iCbCr,x+org_x,y+org_y)=
          ((8-xFrac)*(8-yFrac)*b.p[y]  [x]  +
              xFrac *(8-yFrac)*b.p[y]  [x+1]+
           (8-xFrac)*   yFrac *b.p[y+1][x]  +
              xFrac *   yFrac *b.p[y+1][x+1]+
         32)>>6;
  }
}

void MotionCompensateMB(frame *this, frame *ref,
                        mode_pred_info *mpi,
                        int org_x, int org_y) {
  int x,y;
  for(y=0; y<4; ++y)
    for(x=0; x<4; ++x)
      MotionCompensateTB(this,ref,
        org_x|(x<<2), org_y|(y<<2),
        ModePredInfo_MVx(mpi,(org_x>>2)+x,(org_y>>2)+y),
        ModePredInfo_MVy(mpi,(org_x>>2)+x,(org_y>>2)+y)
      );
}
