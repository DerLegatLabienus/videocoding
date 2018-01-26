#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "common.h"
#include "coretrans.h"

void enter_luma_block(int *scan, frame *f, int x, int y, int qp, int without_dc);
void enter_chroma_block(int *scan, frame *f, int iCbCr, int x, int y, int qp, int without_dc);

void transform_luma_dc(int *scan, int *out, int qp);
void transform_chroma_dc(int *scan, int qp);

core_block coeff_scan(int *scan);

#endif /*__BLOCK_H__*/
