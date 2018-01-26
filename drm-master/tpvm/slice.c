#include "common.h"
#include "input.h"
#include "cavlc.h"
#include "params.h"
#include "mbmodes.h"
#include "residual.h"
#include "mode_pred.h"
#include "intra_pred.h"
#include "mocomp.h"
#include "block.h"
#include "slicehdr.h"
#include "slice.h"

#include <linux/string.h> // memset
#include <linux/slab.h>

int debug = 0;

extern int frame_no;
extern unsigned char nal_buf[NAL_BUF_SIZE];
extern int nal_pos;

// needed from mbmodes.c
extern int CodedBlockPatternMapping_Intra4x4[];
extern int CodedBlockPatternMapping_Inter[];

int Intra4x4ScanOrder[16][2]={
  { 0, 0},  { 4, 0},  { 0, 4},  { 4, 4},
  { 8, 0},  {12, 0},  { 8, 4},  {12, 4},
  { 0, 8},  { 4, 8},  { 0,12},  { 4,12},
  { 8, 8},  {12, 8},  { 8,12},  {12,12}
};

int QPcTable[22]=
  {29,30,31,32,32,33,34,34,35,35,36,36,37,37,37,38,38,38,39,39,39,39};


// some macros for easier access to the various ModePredInfo structures
#define LumaDC_nC     get_luma_nC(mpi,mb_pos_x,mb_pos_y)
#define LumaAC_nC     get_luma_nC(mpi,mb_pos_x+Intra4x4ScanOrder[i8x8*4+i4x4][0],mb_pos_y+Intra4x4ScanOrder[i8x8*4+i4x4][1])
#define ChromaDC_nC   -1
#define ChromaAC_nC   get_chroma_nC(mpi,mb_pos_x+(i4x4&1)*8,mb_pos_y+(i4x4>>1)*8,iCbCr)
#define LumaAdjust    ModePredInfo_TotalCoeffL(mpi,(mb_pos_x+Intra4x4ScanOrder[i8x8*4+i4x4][0])>>2,(mb_pos_y+Intra4x4ScanOrder[i8x8*4+i4x4][1])>>2) =
#define ChromaAdjust  ModePredInfo_TotalCoeffC(mpi,(mb_pos_x+(i4x4&1)*8)>>3,(mb_pos_y+(i4x4>>1)*8)>>3,iCbCr) =
#define Intra4x4PredMode(i) ModePredInfo_Intra4x4PredMode(mpi,(mb_pos_x+Intra4x4ScanOrder[i][0])>>2,(mb_pos_y+Intra4x4ScanOrder[i][1])>>2)

int decode_slice_data_ctr = 0;

// transform coefficient levels  
int *LumaDCLevel;   // === Intra16x16DCLevel
int *LumaACLevel;   // === Intra16x16ACLevel
int *ChromaDCLevel;
int *ChromaACLevel;

#define LumaDCLevelSize   (    16*sizeof(int))
#define LumaACLevelSize   ( 16*16*sizeof(int))
#define ChromaDCLevelSize (   2*4*sizeof(int))
#define ChromaACLevelSize (16*2*4*sizeof(int))

// macros for easy 2d/3d access into the levels buffers
#define LAO(a,b)   (  16*(a) +    (b))
#define CDO(a,b)   (   4*(a) +    (b))
#define CAO(a,b,c) (16*4*(a) + 16*(b))

void clear_levels(void) {
    if (debug) printk(KERN_INFO "clear LumaDCLevel\n");
    memset(LumaDCLevel,   0, LumaDCLevelSize);

    if (debug) printk(KERN_INFO "clear LumaACLevel\n");
    memset(LumaACLevel,   0, LumaACLevelSize);

    if (debug) printk(KERN_INFO "clear ChromaDCLevel\n");
    memset(ChromaDCLevel, 0, ChromaDCLevelSize);

    if (debug) printk(KERN_INFO "clear ChromaACLevel\n");
    memset(ChromaACLevel, 0, ChromaACLevelSize);

    if (debug) printk(KERN_INFO "clear_levels done\n");
}

void init_levels(void) {
  LumaDCLevel   = kmalloc(LumaDCLevelSize,   GFP_KERNEL);
  LumaACLevel   = kmalloc(LumaACLevelSize,   GFP_KERNEL);
  ChromaDCLevel = kmalloc(ChromaDCLevelSize, GFP_KERNEL);
  ChromaACLevel = kmalloc(ChromaACLevelSize, GFP_KERNEL);
}

void free_levels(void) {
  kfree(LumaDCLevel);
  kfree(LumaACLevel);
  kfree(ChromaDCLevel);
  kfree(ChromaACLevel);
}


///////////////////////////////////////////////////////////////////////////////
void decode_slice_data(slice_header *sh,
                       seq_parameter_set *sps, pic_parameter_set *pps,
                       nal_unit *nalu,
                       frame *this, frame *ref,
                       mode_pred_info *mpi, rc4_state* rc4) {

  int CurrMbAddr=sh->first_mb_in_slice*(1+sh->MbaffFrameFlag);
  int moreDataFlag=1;
  int prevMbSkipped=0;
  int MbCount=mpi->MbWidth*mpi->MbHeight;
  int mb_skip_run;
  int mb_qp_delta;
  int QPy,QPc;
  int intra_chroma_pred_mode=0;
  int mb_pos_x, mb_pos_y;

  mb_mode mb;
  sub_mb_mode sub[4];

#if 1
  // clear the frame -- this is only useful for debugging purposes, it may
  // safely be disabled later
  memset(this->L,0,this->Lheight*this->Lpitch);
  memset(this->C[0],128,this->Cheight*this->Cpitch);
  memset(this->C[1],128,this->Cheight*this->Cpitch);
#endif

  // initialize some values
  clear_mode_pred_info(mpi);
  QPy=sh->SliceQPy;
  QPc=QPy;  // only to prevent a warning


  moreDataFlag=more_rbsp_data(nalu);
  while(moreDataFlag && CurrMbAddr<MbCount) {
    // mb_skip_run ////////////////////////////////////////////////////////////
    if(sh->slice_type!=I_SLICE && sh->slice_type!=SI_SLICE) {
      mb_skip_run=get_unsigned_exp_golomb();
      prevMbSkipped=(mb_skip_run>0);
      for(; mb_skip_run; --mb_skip_run, ++CurrMbAddr) {
        if(CurrMbAddr>=MbCount) return;
        mb_pos_x=CurrMbAddr%sps->PicWidthInMbs;
        mb_pos_y=CurrMbAddr/sps->PicWidthInMbs;
        ModePredInfo_MbMode(mpi,mb_pos_x,mb_pos_y)=P_Skip;
        mb_pos_x<<=4; mb_pos_y<<=4;
//printf("\n[%d:%d] %d,%d MB: P_Skip\n",frame_no,CurrMbAddr,mb_pos_x,mb_pos_y);
        Derive_P_Skip_MVs(mpi,mb_pos_x,mb_pos_y);
        MotionCompensateMB(this,ref,mpi,mb_pos_x,mb_pos_y);
//L_pixel(this,mb_pos_x+8,mb_pos_y+8)=255;
      }
      moreDataFlag=more_rbsp_data(nalu);
    }
    if(CurrMbAddr>=MbCount) return;

    if(moreDataFlag) {  // macroblock_layer() /////////////////////////////////
      decode_mb_mode(&mb,sh->slice_type,get_unsigned_exp_golomb());
      mb_pos_x=CurrMbAddr%sps->PicWidthInMbs;
      mb_pos_y=CurrMbAddr/sps->PicWidthInMbs;
      ModePredInfo_MbMode(mpi,mb_pos_x,mb_pos_y)=mb.mb_type;
      mb_pos_x<<=4; mb_pos_y<<=4;
      /* if (frame_no > 1) { */
      /*     debug = 1; */
      /* } */
//printf("\n[%d:%d] %d,%d ",frame_no,CurrMbAddr,mb_pos_x,mb_pos_y);_dump_mb_mode(&mb);
      
      if(mb.mb_type==I_PCM) {  // I_PCM macroblock ////////////////////////////
        int x,y,iCbCr;
        unsigned char *pos;        
        if (debug) printk(KERN_INFO "mb.mb_type==I_PCM\n");
        input_align_to_next_byte();
        pos=&L_pixel(this,mb_pos_x,mb_pos_y);
        for(y=16; y; --y) {
          for(x=16; x; --x)
            *pos++=input_get_byte();
          pos+=this->Lpitch-16;
        }
        for(iCbCr=0; iCbCr<2; ++iCbCr) {
          pos=&C_pixel(this,iCbCr,mb_pos_x>>1,mb_pos_y>>1);
          for(y=8; y; --y) {
            for(x=8; x; --x)
              *pos++=input_get_byte();
            pos+=this->Cpitch-8;
          }
        }
        // fix mode_pred_info->TotalCoeff data
        for(y=0; y<4; ++y)
          for(x=0; x<4; ++x)
            ModePredInfo_TotalCoeffL(mpi,(mb_pos_x>>2)+x,(mb_pos_y>>2)+y)=16;
        for(y=0; y<2; ++y)
          for(x=0; x<2; ++x) {
            ModePredInfo_TotalCoeffC(mpi,(mb_pos_x>>3)+x,(mb_pos_y>>3)+y,0)=16;
            ModePredInfo_TotalCoeffC(mpi,(mb_pos_x>>3)+x,(mb_pos_y>>3)+y,1)=16;
          }
      } else {  // "normal" macroblock ////////////////////////////////////////
          if (debug) printk(KERN_INFO "\"normal\" macroblock\n");

        if(mb.MbPartPredMode[0]!=Intra_4x4 &&
           mb.MbPartPredMode[0]!=Intra_16x16 &&
           mb.NumMbPart==4)
        { // sub_mb_pred() ////////////////////////////////////////////////////
          int mbPartIdx,subMbPartIdx;
          for(mbPartIdx=0; mbPartIdx<4; ++mbPartIdx)
            decode_sub_mb_mode(&sub[mbPartIdx],sh->slice_type,
                               get_unsigned_exp_golomb());
//{int i;printf("sub_mb_pred():");for(i=0;i<4;++i)printf(" %s(%d)",_str_sub_mb_type(sub[i].sub_mb_type),sub[i].NumSubMbPart);printf("\n");}
          // ignoring ref_idx_* and *_l1 stuff for now -- I do not support
          // long-term prediction or B-frames anyway ...
          for(mbPartIdx=0; mbPartIdx<4; ++mbPartIdx)
            if(sub[mbPartIdx].sub_mb_type!=B_Direct_8x8 &&
               sub[mbPartIdx].SubMbPredMode!=Pred_L1)
            { // SOF = "scan order factor"
              int SOF=(sub[mbPartIdx].sub_mb_type==P_L0_8x4)?2:1;
              for(subMbPartIdx=0; subMbPartIdx<sub[mbPartIdx].NumSubMbPart; ++subMbPartIdx) {
                int mvdx=get_signed_exp_golomb();
                int mvdy=get_signed_exp_golomb();
                //mvdx *= -1;
                //printf("dump mv1: %d:%d\n", mvdx, mvdy);

                DeriveMVs(mpi,
                  mb_pos_x+Intra4x4ScanOrder[mbPartIdx*4+subMbPartIdx*SOF][0],
                  mb_pos_y+Intra4x4ScanOrder[mbPartIdx*4+subMbPartIdx*SOF][1],
                  sub[mbPartIdx].SubMbPartWidth,
                  sub[mbPartIdx].SubMbPartHeight,
                  mvdx, mvdy);
              }
            }
        } else {  // mb_pred() ////////////////////////////////////////////////
            if (debug) {
                int i, j;
                printk(KERN_INFO "mb_pred(), mb_pos_x=%d, mb_pos_y=%d\n", mb_pos_x, mb_pos_y);
                printk(KERN_INFO "nal_pos = %d\n", nal_pos);
                for (i=0; i < 4; ++i) {
                    char buffer[30];
                    for (j=0; j < 8; ++j) {
                        sprintf(buffer + 3*j, "%2x|", *(nal_buf + nal_pos + i*8+j));
                    }
                    printk(KERN_INFO "%s\n", buffer);
                }
            }

          if(mb.MbPartPredMode[0]==Intra_4x4 ||
             mb.MbPartPredMode[0]==Intra_16x16)
          {  // mb_pred() for intra macroblocks ///////////////////////////////
            if(mb.MbPartPredMode[0]==Intra_4x4) {
              int luma4x4BlkIdx;
//printf("intra_4x4:");
              for(luma4x4BlkIdx=0; luma4x4BlkIdx<16; ++luma4x4BlkIdx) {
                int predIntra4x4PredMode=get_predIntra4x4PredMode(mpi,
                      mb_pos_x+Intra4x4ScanOrder[luma4x4BlkIdx][0],
                      mb_pos_y+Intra4x4ScanOrder[luma4x4BlkIdx][1]);
                if (debug) printk(KERN_INFO  "predIntra4x4PredMode=%d\n", predIntra4x4PredMode);
                if(input_get_one_bit()) { // prev_intra4x4_pred_mode_flag
                    if (debug) printk(KERN_INFO "prev_intra4x4_pred_mode_flag set\n");
                /* #define Intra4x4PredMode(i) ModePredInfo_Intra4x4PredMode(mpi, \ */
                /*    (mb_pos_x+Intra4x4ScanOrder[i][0])>>2,                      \ */
                /*    (mb_pos_y+Intra4x4ScanOrder[i][1])>>2) */
                /* #define ModePredInfo_Intra4x4PredMode(mpi,x,y) (mpi->Intra4x4PredMode[(y)*mpi->TbPitch+(x)]) */

                  Intra4x4PredMode(luma4x4BlkIdx)=predIntra4x4PredMode;
                  /* fprintf(stderr, "p "); */
                }
                  
                else {
                  int rem_intra4x4_pred_mode=input_get_bits(3);
                    if (debug) printk(KERN_INFO "prev_intra4x4_pred_mode_flag not set, rem = %d\n", rem_intra4x4_pred_mode);
                  /* fprintf(stderr, "r:%d ", rem_intra4x4_pred_mode); */
                  if(rem_intra4x4_pred_mode<predIntra4x4PredMode)
                    Intra4x4PredMode(luma4x4BlkIdx)=rem_intra4x4_pred_mode;
                  else
                    Intra4x4PredMode(luma4x4BlkIdx)=rem_intra4x4_pred_mode+1;
                }
              }
            }
            intra_chroma_pred_mode=get_unsigned_exp_golomb();
            if (debug) printk(KERN_INFO "intra_chroma_pred_mode = %d\n", intra_chroma_pred_mode);
          } else { // mb_pred() for inter macroblocks /////////////////////////
            int mbPartIdx;
            int SOF=(mb.mb_type==P_L0_L0_16x8)?8:4;
            if (debug) printk(KERN_INFO "mb_pred() for inter macroblock\n");
            // ignoring ref_idx_* and *_l1 stuff for now -- I do not support
            // long-term prediction or B-frames anyway ...
            for(mbPartIdx=0; mbPartIdx<mb.NumMbPart; ++mbPartIdx)
              if(mb.MbPartPredMode[mbPartIdx]!=Pred_L1) {
                int mvdx=get_signed_exp_golomb();
                int mvdy=get_signed_exp_golomb();
                if (debug) printk(KERN_INFO "mvdx=%d, mvdy=%d\n", mvdx, mvdy);
                //mvdx *= -1;
                //printf("dump mv2: %d:%d\n", mvdx, mvdy);
                DeriveMVs(mpi,
                  mb_pos_x+Intra4x4ScanOrder[mbPartIdx*SOF][0],
                  mb_pos_y+Intra4x4ScanOrder[mbPartIdx*SOF][1],
                  mb.MbPartWidth, mb.MbPartHeight, mvdx, mvdy);
              }
            }
        }

        // coded_block_pattern ////////////////////////////////////////////////
        if(mb.MbPartPredMode[0]!=Intra_16x16) {
          int coded_block_pattern=get_unsigned_exp_golomb();
          if(mb.MbPartPredMode[0]==Intra_4x4)
            coded_block_pattern=CodedBlockPatternMapping_Intra4x4[coded_block_pattern];
          else
            coded_block_pattern=CodedBlockPatternMapping_Inter[coded_block_pattern];
          if (debug) printk("coded_block_pattern = %d\n", coded_block_pattern);
//printf("coded_block_pattern=%d\n",coded_block_pattern);
          mb.CodedBlockPatternLuma=coded_block_pattern&15;
          mb.CodedBlockPatternChroma=coded_block_pattern>>4;
//_dump_mb_mode(&mb);
        }

        // Before parsing the residual data, set all coefficients to zero. In
        // the original H.264 documentation, this is done either in
        // residual_block() at the very beginning or by setting values to zero
        // according to the CodedBlockPattern values. So, there's only little
        // overhead if we do it right here.
        clear_levels();

        // residual() /////////////////////////////////////////////////////////
        if (debug) printk(KERN_INFO "residual()\n");
        if(mb.CodedBlockPatternLuma>0 || mb.CodedBlockPatternChroma>0 ||
           mb.MbPartPredMode[0]==Intra_16x16)
        {
          int i8x8,i4x4,iCbCr,QPi;

          mb_qp_delta=get_signed_exp_golomb();
          QPy=(QPy+mb_qp_delta+52)%52;
          QPi=QPy+pps->chroma_qp_index_offset;
          QPi=CustomClip(QPi,0,51);
          if(QPi<30) QPc=QPi;
                else QPc=QPcTable[QPi-30];

          // OK, now let's parse the hell out of the stream ;)        
          if (debug) printk(KERN_INFO "let's parse the hell out of the stream\n");
          if(mb.MbPartPredMode[0]==Intra_16x16) {
              //printk(KERN_INFO "residual_block(&LumaDCLevel[0],16,LumaDC_nC, rc4)\n");
              residual_block(&LumaDCLevel[0],16,LumaDC_nC, rc4);
          }
          if (debug) printk(KERN_INFO "residual 1\n");
          for(i8x8=0; i8x8<4; ++i8x8)
            for(i4x4=0; i4x4<4; ++i4x4)
              if(mb.CodedBlockPatternLuma&(1<<i8x8)) {
                if(mb.MbPartPredMode[0]==Intra_16x16) {
                  //printk(KERN_INFO "LumaAdjust residual_block(&LumaACLevel[i8x8*4+i4x4][1],15,LumaAC_nC, rc4)\n");
                  LumaAdjust residual_block(&LumaACLevel[LAO(i8x8*4+i4x4,1)],15,LumaAC_nC, rc4);
                }
                else {
                  //printk(KERN_INFO "LumaAdjust residual_block(&LumaACLevel[i8x8*4+i4x4][0],16,LumaAC_nC, rc4)\n");
                  LumaAdjust residual_block(&LumaACLevel[LAO(i8x8*4+i4x4,0)],16,LumaAC_nC, rc4);
                }
              };
          if (debug) printk(KERN_INFO "residual 2\n");
          for(iCbCr=0; iCbCr<2; iCbCr++)
            if(mb.CodedBlockPatternChroma&3) {
              //printk(KERN_INFO "residual_block(&ChromaDCLevel[%d][0],4,ChromaDC_nC, rc4)\n", iCbCr);
              residual_block(&ChromaDCLevel[CDO(iCbCr,0)],4,ChromaDC_nC, rc4);
            }
          if (debug) printk(KERN_INFO "residual 3\n");
          for(iCbCr=0; iCbCr<2; iCbCr++)
            for(i4x4=0; i4x4<4; ++i4x4)
              if(mb.CodedBlockPatternChroma&2) {
                //printk(KERN_INFO "ChromaAdjust residual_block(&ChromaACLevel[iCbCr][i4x4][1],15,ChromaAC_nC, rc4)\n");
                ChromaAdjust residual_block(&ChromaACLevel[CAO(iCbCr,i4x4,1)],15,ChromaAC_nC, rc4);
              }
/*
{ int i; printf("L:"); for(i=0; i<16; ++i) printf(" %d",LumaDCLevel[i]); printf("\n");
for(i8x8=0; i8x8<16; ++i8x8) { printf("  [%2d]",i8x8);
for(i=0; i<16; ++i) printf(" %d",LumaACLevel[i8x8][i]); printf("\n"); }
printf("Cb:"); for(i=0; i<4; ++i) printf(" %d",ChromaDCLevel[0][i]); printf("\n");
for(i8x8=0; i8x8<4; ++i8x8) { printf("  [%d]",i8x8);
for(i=0; i<16; ++i) printf(" %d",ChromaACLevel[0][i8x8][i]); printf("\n"); }
printf("Cr:"); for(i=0; i<4; ++i) printf(" %d",ChromaDCLevel[1][i]); printf("\n");
for(i8x8=0; i8x8<4; ++i8x8) { printf("  [%d]",i8x8);
for(i=0; i<16; ++i) printf(" %d",ChromaACLevel[1][i8x8][i]); printf("\n"); } }
*/
        }


        //////////////////////////// RENDERING ////////////////////////////////
        // Now that we have all the informations needed about this macroblock,
        // we can go ahead and really render it.

        if (debug) printk("RENDERING\n");
        if(mb.MbPartPredMode[0]==Intra_4x4) {  ///////////////// Intra_4x4_Pred
          int i;
          if (debug) printk(KERN_INFO "Intra_4x4_Pred\n");
          for(i=0; i<16; ++i) {
            int x=mb_pos_x+Intra4x4ScanOrder[i][0];
            int y=mb_pos_y+Intra4x4ScanOrder[i][1];
            perf_enter("intra prediction");
            Intra_4x4_Dispatch(this,mpi,x,y,i);
            perf_enter("block entering");
            enter_luma_block(&LumaACLevel[LAO(i,0)],this,x,y,QPy,0);
            /* if (debug) { */
            /*     int j; */
            /*     printk(KERN_INFO "LumaACLevel: "); */
            /*     for (j=0; j<16; ++j) */
            /*         printk(KERN_INFO "%d, ", LumaACLevel[LAO(i,j)]); */
            /*     printk(KERN_INFO "\n"); */
            /* } */

          }
          Intra_Chroma_Dispatch(this,mpi,intra_chroma_pred_mode,mb_pos_x>>1,mb_pos_y>>1,pps->constrained_intra_pred_flag);
        } else if(mb.MbPartPredMode[0]==Intra_16x16) {  ////// Intra_16x16_Pred
          int i,j;
          if (debug) printk(KERN_INFO "Intra_16x16_Pred\n");
          perf_enter("intra prediction");
          Intra_16x16_Dispatch(this,mpi,mb.Intra16x16PredMode,mb_pos_x,mb_pos_y,pps->constrained_intra_pred_flag);
          perf_enter("block entering");
          transform_luma_dc(&LumaDCLevel[0],&LumaACLevel[LAO(0,0)],QPy);
          for(i=0; i<16; ++i) {
            int x=mb_pos_x+Intra4x4ScanOrder[i][0];
            int y=mb_pos_y+Intra4x4ScanOrder[i][1];
            enter_luma_block(&LumaACLevel[LAO(i,0)],this,x,y,QPy,1);
          }
          perf_enter("block entering");
          Intra_Chroma_Dispatch(this,mpi,intra_chroma_pred_mode,mb_pos_x>>1,mb_pos_y>>1,pps->constrained_intra_pred_flag);
          // act as if all transform blocks inside this macroblock were
          // predicted using the Intra_4x4_DC prediction mode
          // (without constrained_intra_pred, we'd have to do the same for
          // inter MBs)
          for(i=0; i<4; ++i) for(j=0; j<4; ++j)
            ModePredInfo_Intra4x4PredMode(mpi,(mb_pos_x>>2)+j,(mb_pos_y>>2)+i)=2;
        } else 
        { ///////////////////////////////////////////////// Inter_*_Pred
          int i;
          if (debug) printk(KERN_INFO "Inter_*_Pred\n");
/*
{int x,y;for(y=0;y<4;++y){for(x=0;x<4;++x){i=((mb_pos_y>>2)+y)*mpi->TbPitch+(mb_pos_x>>2)+x;
printf("|%4d,%-4d",mpi->MVx[i],mpi->MVy[i]);}printf("\n");}}
*/
          perf_enter("inter prediction");
          MotionCompensateMB(this,ref,mpi,mb_pos_x,mb_pos_y);
          perf_enter("block entering");
          for(i=0; i<16; ++i) {
            int x=mb_pos_x+Intra4x4ScanOrder[i][0];
            int y=mb_pos_y+Intra4x4ScanOrder[i][1];
            enter_luma_block(&LumaACLevel[LAO(i,0)],this,x,y,QPy,0);
          }
        } /*else
          printf("unsupported prediction mode at %d,%d!\n",mb_pos_x,mb_pos_y);*/

        if(mb.CodedBlockPatternChroma) { ////////////////////// Chroma Residual
          int iCbCr,i;
          if (debug) printk(KERN_INFO "Chroma Residual");
          perf_enter("block entering");
          for(iCbCr=0; iCbCr<2; ++iCbCr) {
            transform_chroma_dc(&ChromaDCLevel[CDO(iCbCr,0)],QPc);
            for(i=0; i<4; ++i)
              ChromaACLevel[CAO(iCbCr,i,0)]=ChromaDCLevel[CDO(iCbCr,i)];
            for(i=0; i<4; ++i)
              enter_chroma_block(&ChromaACLevel[CAO(iCbCr,i,0)],this,iCbCr,
                (mb_pos_x>>1)+Intra4x4ScanOrder[i][0],
                (mb_pos_y>>1)+Intra4x4ScanOrder[i][1],
              QPc,1);
          }
        }
      }

//if(frame_no>=1 && CurrMbAddr>=0) { L_pixel(this,mb_pos_x+15,mb_pos_y+15)=255; return; }
    } ///////////// end of macroblock_layer() /////////////////////////////////

    moreDataFlag=more_rbsp_data(nalu);
    ++CurrMbAddr;
  } /* while(moreDataFlag && CurrMbAddr<=MbCount) */

}
