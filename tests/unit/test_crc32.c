#include "test.h"

#include <stdint.h>
#include <string.h>

#include "npbc/crc32.h"

int npbc_test_crc32(void) {
    const char vector[] = "123456789";
    NpbcCrc32 crc;

    NPBC_ASSERT(npbc_crc32(NULL, 0) == 0);
    NPBC_ASSERT(
        npbc_crc32(vector, strlen(vector)) == UINT32_C(0xcbf43926)
    );

    npbc_crc32_init(&crc);
    npbc_crc32_update(&crc, vector, 4);
    npbc_crc32_update(&crc, vector + 4, 5);
    NPBC_ASSERT(npbc_crc32_finish(&crc) == UINT32_C(0xcbf43926));

    npbc_crc32_init(&crc);
    npbc_crc32_update(&crc, NULL, 0);
    NPBC_ASSERT(npbc_crc32_finish(&crc) == 0);
    NPBC_ASSERT(npbc_crc32_finish(NULL) == 0);
    return 0;
}
