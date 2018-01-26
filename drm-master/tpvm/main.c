#include "common.h"
#include "input.h"
#include "nal.h"
#include "cavlc.h"
#include "params.h"
#include "slicehdr.h"
#include "slice.h"
#include "residual.h"
#include "mode_pred.h" 
#include "main.h"

#include <linux/kernel.h>
#include <linux/slab.h>

frame *this=NULL, *ref=NULL;
mode_pred_info *mpi=NULL;
int frame_no;
nal_unit nalu;
seq_parameter_set sps;
pic_parameter_set pps;
slice_header sh;

int h264_open(const char *filename) {
  int have_sps=0,have_pps=0;
  if(!input_open(filename)) {
    printk(KERN_ERR "H.264 Error: Cannot open input file!\n");
    return 0;
  }
  init_code_tables();
  init_levels();
  frame_no=0;
  while(get_next_nal_unit(&nalu)) {
    switch(nalu.nal_unit_type) {
      case 7:  // sequence parameter set //
        if(have_sps)
          printk(KERN_ERR "H.264 Warning: Duplicate sequence parameter set, skipping!\n");
        else {
          decode_seq_parameter_set(&sps);
          have_sps=1;
        }
        break;
      case 8:  // picture parameter set //
        if(!have_sps)
          printk(KERN_ERR "H.264 Warning: Picture parameter set without sequence parameter set, skipping!\n");
        else if(have_pps)
          printk(KERN_ERR "H.264 Warning: Duplicate picture parameter set, skipping!\n");
        else {
          decode_pic_parameter_set(&pps);
          have_pps=1;
          if(check_unsupported_features(&sps,&pps)) {
            printk(KERN_ERR "H.264 Error: Unsupported features found in headers!\n");
            input_close();
            return 0;
          }
          this=alloc_frame(sps.PicWidthInSamples,sps.FrameHeightInSamples);
          ref=alloc_frame(sps.PicWidthInSamples,sps.FrameHeightInSamples);
          mpi=alloc_mode_pred_info(sps.PicWidthInSamples,sps.FrameHeightInSamples);
          return (sps.FrameHeightInSamples<<16)|sps.PicWidthInSamples;
        }
        break;
      case 1: case 5:  // coded slice of a picture //
        printk(KERN_ERR "H.264 Warning: Pictures sent before headers!\n");
        break;
      default:  // unsupported NAL unit type //
        printk(KERN_ERR "H.264 Warning: NAL unit with unsupported type, skipping!\n");
    }
  }
  printk(KERN_ERR "H.264 Error: Unexpected end of file!\n");
  return 0;
}

struct rc4;
frame *h264_decode_frame(int verbose, rc4_state* rc4) {
  while(get_next_nal_unit(&nalu)) {
    if(nalu.nal_unit_type==1 || nalu.nal_unit_type==5) {
      //perf_enter("slice decoding");
      ++frame_no;
      decode_slice_header(&sh,&sps,&pps,&nalu);
      if(verbose)
        printk(KERN_INFO "Frame%4d: %s\n",frame_no,_str_slice_type(sh.slice_type));
      if(sh.slice_type!=I_SLICE && sh.slice_type!=P_SLICE)
        printk(KERN_ERR "**** H.264 Warning: Unsupported slice type (%s), skipping!\n",
                       _str_slice_type(sh.slice_type));
      else {
        frame *temp;
        decode_slice_data(&sh,&sps,&pps,&nalu,this,ref,mpi, rc4);
        temp=this; this=ref; ref=temp;
        return temp;
      }
    } else if(nalu.nal_unit_type!=6 && nalu.nal_unit_type!=7 && nalu.nal_unit_type!=8) {
      printk(KERN_ERR "H.264 Warning: unexpected or unsupported NAL unit type %d!\n", nalu.nal_unit_type);
    } 
  }
  return NULL;
}

void h264_rewind() {
  input_rewind();
  frame_no=0;
}

void h264_close() {
  free_frame(this);
  free_frame(ref);
  free_mode_pred_info(mpi);
  input_close();
  free_levels();
}


inline int h264_frame_no() {
  return frame_no;
}


///////////////////////////////////////////////////////////////////////////////

#ifdef BUILD_TESTS

int _test_main(int argc, char *argv[]) {
  FILE *out;
  frame *f;
  int info;

  info=h264_open("../streams/in.264");
  if(!info) return 1;

  if(!(out=fopen("../streams/out","wb"))) {
    printk(KERN_ERR "Error: Cannot open output file!\n");
    return 1;
  }

  printk(KERN_INFO "H.264 stream, %dx%d pixels\n",H264_WIDTH(info),H264_HEIGHT(info));

  while((f=h264_decode_frame(1))) {
    printk(KERN_INFO "\n");
    fwrite(f->L,f->Lpitch,f->Lheight,out);
    fwrite(f->C[0],f->Cpitch,f->Cheight,out);
    fwrite(f->C[1],f->Cpitch,f->Cheight,out);
if(frame_no>=1) break;
  }
  
  fclose(out);
  h264_close();
  return 0;
}

#endif
