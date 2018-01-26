#ifndef __SLICE_H__
#define __SLICE_H__

#include "params.h"
#include "nal.h"
#include "slicehdr.h"
#include "common.h"
#include "mode_pred.h"
// #include "enc.h"
#include "rc4.h"

void decode_slice_data(slice_header *sh,
                       seq_parameter_set *sps, pic_parameter_set *pps,
                       nal_unit *nalu,
                       frame *this, frame *ref,
                       mode_pred_info *mpi, rc4_state* rc4);

void init_levels(void);
void clear_levels(void);

#endif /*__SLICE_H__*/
