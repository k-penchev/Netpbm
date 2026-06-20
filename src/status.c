#include "npbc/status.h"

const char *npbc_status_name(NpbcStatus status) {
    switch (status) {
        case NPBC_STATUS_OK:
            return "ok";
        case NPBC_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case NPBC_STATUS_INVALID_FORMAT:
            return "invalid format";
        case NPBC_STATUS_UNSUPPORTED:
            return "unsupported data";
        case NPBC_STATUS_SIZE_OVERFLOW:
            return "size overflow";
        case NPBC_STATUS_ALLOCATION_FAILED:
            return "allocation failed";
        case NPBC_STATUS_IO_FAILED:
            return "I/O failed";
        case NPBC_STATUS_TRUNCATED_INPUT:
            return "truncated input";
        case NPBC_STATUS_INTEGRITY_FAILED:
            return "integrity failed";
        case NPBC_STATUS_OUTPUT_EXISTS:
            return "output exists";
        case NPBC_STATUS_INTERNAL_ERROR:
            return "internal error";
    }

    return "unknown status";
}
