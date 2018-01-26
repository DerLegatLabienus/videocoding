#include "common.h"
#include "common.h"
#include "coretrans.h"
#include "block.h"

void enter_luma_block(int *scan, frame *f, int x, int y, int qp, int without_dc) {
  direct_ict(inverse_quantize(coeff_scan(scan),qp,without_dc),
             &L_pixel(f,x,y),f->Lpitch);
}

void enter_chroma_block(int *scan, frame *f, int iCbCr, int x, int y, int qp, int without_dc) {
  direct_ict(inverse_quantize(coeff_scan(scan),qp,without_dc),
             &C_pixel(f,iCbCr,x,y),f->Cpitch);
}


CONST int LevelScale[6]={10,11,13,14,16,18};

void transform_luma_dc(int *scan, int *out, int qp) {
  CONST int ScanOrder[16]={0,1,4,5,2,3,6,7,8,9,12,13,10,11,14,15};
  core_block block=hadamard(coeff_scan(scan));
  int scale=LevelScale[qp%6];
  int i;
  if(qp>=12)
    for(i=0; i<16; ++i)
      out[ScanOrder[i]<<4]=(block.items[i]*scale)<<(qp/6-2);
  else {
    int round_adj=1<<(1-qp/6);
    for(i=0; i<16; ++i)
      out[ScanOrder[i]<<4]=(block.items[i]*scale+round_adj)>>(2-qp/6);
  }
}

void transform_chroma_dc(int *scan, int qp) {
  int scale=LevelScale[qp%6];
  int a=scan[0]+scan[1]+scan[2]+scan[3];
  int b=scan[0]-scan[1]+scan[2]-scan[3];
  int c=scan[0]+scan[1]-scan[2]-scan[3];
  int d=scan[0]-scan[1]-scan[2]+scan[3];
  if(qp>=6) {
    scan[0]=(a*scale)<<(qp/6-1);
    scan[1]=(b*scale)<<(qp/6-1);
    scan[2]=(c*scale)<<(qp/6-1);
    scan[3]=(d*scale)<<(qp/6-1);
  } else {
    scan[0]=(a*scale)>>1;
    scan[1]=(b*scale)>>1;
    scan[2]=(c*scale)>>1;
    scan[3]=(d*scale)>>1;
  }
}

CONST int ZigZagOrder[]={0,1,4,8,5,2,3,6,9,12,13,10,7,11,14,15};

core_block coeff_scan(int *scan) {
  core_block res;
  int i;
  for(i=0; i<16; ++i)
    res.items[ZigZagOrder[i]]=scan[i];
  return res;
}
