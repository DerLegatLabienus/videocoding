#include "protocol.h"

char* protoAsString(int val) {
    switch (val) {
        case NA: return "NA";
        case NEW_DATA_AVAILABLE: return "NEW_DATA_AVAILABLE";
        case VIDEO_DONE: return "VIDEO_DONE";
        case READY_FOR_NEW_DATA: return "READY_FOR_NEW_DATA";
        case ABORT: return "ABORT";
        default: return "!!undefined!!";
    }
}
