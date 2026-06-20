#include "npbc/crc32.h"

#include <stdint.h>

void npbc_crc32_init(NpbcCrc32 *crc) {
    if (crc != NULL) {
        crc->value = UINT32_C(0xffffffff);
    }
}

void npbc_crc32_update(NpbcCrc32 *crc, const void *data, size_t size) {
    const uint8_t *bytes;
    size_t index;

    if (crc == NULL || (data == NULL && size != 0)) {
        return;
    }

    bytes = data;
    for (index = 0; index < size; index++) {
        unsigned int bit;

        crc->value ^= bytes[index];
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = (uint32_t)(0 - (crc->value & 1U));

            crc->value = (crc->value >> 1)
                ^ (UINT32_C(0xedb88320) & mask);
        }
    }
}

uint32_t npbc_crc32_finish(const NpbcCrc32 *crc) {
    if (crc == NULL) {
        return 0;
    }
    return crc->value ^ UINT32_C(0xffffffff);
}

uint32_t npbc_crc32(const void *data, size_t size) {
    NpbcCrc32 crc;

    npbc_crc32_init(&crc);
    npbc_crc32_update(&crc, data, size);
    return npbc_crc32_finish(&crc);
}
