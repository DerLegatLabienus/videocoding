#ifndef __RESIDUAL_H__
#define __RESIDUAL_H__
#include "rc4.h"

void init_code_tables();

// parse transform coefficient level from the data stream into arrays
int residual_block(int *coeffLevel, int maxNumCoeff, int nC, rc4_state* rc4);

#endif /*__RESIDUAL_H__*/
