#ifndef __INTRA_PRED_H__
#define __INTRA_PRED_H__

#include "common.h"
#include "mode_pred.h"

void Intra_4x4_Dispatch   (frame *f, mode_pred_info *mpi,           int x, int y, int luma4x4BlkIdx);
void Intra_16x16_Dispatch (frame *f, mode_pred_info *mpi, int mode, int x, int y, int constrained_intra_pred);
void Intra_Chroma_Dispatch(frame *f, mode_pred_info *mpi, int mode, int x, int y, int constrained_intra_pred);

#endif /*__INTRA_PRED_H__*/
