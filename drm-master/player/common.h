#ifndef __COMMON_H__
#define __COMMON_h__

#include "../tpvm/protocol.h"

#define DEBUG

#ifdef DEBUG
# define PRINT(x) printf x
#else
# define PRINT(x) do {} while (0)
#endif

#define BLKS     2
#define BLK_SIZE 4096
#define dev_path "/dev/tpvm"

#define KEYJ_IOC_MAGIC 'k'
#define KEYJ_IOCRENDERER _IOW(KEYJ_IOC_MAGIC, 0, int)
#define KEYJ_IOCLISTENER _IOW(KEYJ_IOC_MAGIC, 1, int)

int write_efds(int efd1, int efd2, const char* path);

int equal(const Metadata, const Metadata);

int is_valid(const Metadata);

#endif
