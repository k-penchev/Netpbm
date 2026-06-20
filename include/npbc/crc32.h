#ifndef NPBC_CRC32_H
#define NPBC_CRC32_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t value;
} NpbcCrc32;

void npbc_crc32_init(NpbcCrc32 *crc);
void npbc_crc32_update(NpbcCrc32 *crc, const void *data, size_t size);
uint32_t npbc_crc32_finish(const NpbcCrc32 *crc);
uint32_t npbc_crc32(const void *data, size_t size);

#endif
