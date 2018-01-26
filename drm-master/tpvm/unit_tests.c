#include "protocol.h"
#include <stdio.h>

int test_pack() {
    int blk_size = 4096;
    uint64_t packed = PACK(NEW_DATA_AVAILABLE, blk_size);

    if (GET_TYPE(packed) != NEW_DATA_AVAILABLE) {
        fprintf(stderr, "test_pack failed! expected GET_TYPE(packed) to be %d but actually %d\n", NEW_DATA_AVAILABLE, GET_TYPE(packed));
        return -1;
    }
    if (GET_VAL(packed) != blk_size)
        return -1;

    return 0;

}

int main(void) {
    if (test_pack() < 0) {
        printf("test_pack failed!\n");
        return -1;
    }

    return 0;

}
