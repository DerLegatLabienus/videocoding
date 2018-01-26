#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#define NA                 0
#define NEW_DATA_AVAILABLE 1
#define VIDEO_DONE         2
#define READY_FOR_NEW_DATA 3
#define ABORT              4

char* protoAsString(int val);

#define GET_TYPE(x) (uint32_t)(x)
#define GET_VAL(x)  (uint32_t)((x) >> 32)

#define PACK(t, v) ((uint64_t)(v)) << 32 | (t);

typedef struct Metadata {
    int width;
    int height;
    int frames;
} Metadata;

                                                                                                                 
#endif
