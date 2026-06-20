#include "test.h"

#include "npbc/cli.h"
#include "npbc/status.h"

int npbc_test_cli(void) {
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_OK) == 0);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_INVALID_FORMAT) == 3);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_UNSUPPORTED) == 3);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_TRUNCATED_INPUT) == 3);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_IO_FAILED) == 4);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_OUTPUT_EXISTS) == 4);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_INTEGRITY_FAILED) == 5);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_SIZE_OVERFLOW) == 6);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_ALLOCATION_FAILED) == 6);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_INVALID_ARGUMENT) == 7);
    NPBC_ASSERT(npbc_cli_exit_code(NPBC_STATUS_INTERNAL_ERROR) == 7);
    NPBC_ASSERT(npbc_cli_exit_code((NpbcStatus)999) == 7);
    return 0;
}
