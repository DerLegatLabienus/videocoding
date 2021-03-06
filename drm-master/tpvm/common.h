#ifndef __COMMON_H__
#define __COMMON_H__

#define USE_X86_ASM

#include "perf.h"
#include "main.h"

#define CONST static const

#define ExtractSign(x) ((x)>>31)
#define CombineSign(sign,value) ((sign)?(-(value)):(value))

frame *alloc_frame(int width, int height);
void free_frame(frame *f);
int abs(int val);

#define CustomClip(i,min,max) (((i)<min)?min:(((i)>max)?max:(i)))
#define Clip(i) CustomClip(i,0,255)

#endif /*__COMMON_H__*/
