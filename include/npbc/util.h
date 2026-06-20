#ifndef NPBC_UTIL_H
#define NPBC_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "npbc/image.h"
#include "npbc/status.h"

#define NPBC_MAX_RASTER_SIZE ((size_t)1073741824)

NpbcStatus npbc_size_add(size_t left, size_t right, size_t *result);
NpbcStatus npbc_size_multiply(size_t left, size_t right, size_t *result);
NpbcStatus npbc_raster_size(
    NpbcImageKind kind,
    uint32_t width,
    uint32_t height,
    uint16_t max_value,
    size_t *result
);
NpbcStatus npbc_read_exact(FILE *stream, void *data, size_t size);
NpbcStatus npbc_write_exact(FILE *stream, const void *data, size_t size);
uint16_t npbc_read_u16_be(const uint8_t data[2]);
uint32_t npbc_read_u32_be(const uint8_t data[4]);
uint64_t npbc_read_u64_be(const uint8_t data[8]);
void npbc_write_u16_be(uint8_t data[2], uint16_t value);
void npbc_write_u32_be(uint8_t data[4], uint32_t value);
void npbc_write_u64_be(uint8_t data[8], uint64_t value);

#endif
