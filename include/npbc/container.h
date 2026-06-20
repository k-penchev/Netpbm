#ifndef NPBC_CONTAINER_H
#define NPBC_CONTAINER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "npbc/compression.h"
#include "npbc/image.h"
#include "npbc/status.h"

#define NPBC_CONTAINER_HEADER_SIZE ((size_t)44)
#define NPBC_CONTAINER_COMPATIBILITY 1

typedef struct {
    uint8_t compatibility;
    NpbcCompressionAlgorithm algorithm;
    NpbcImageKind image_kind;
    uint32_t width;
    uint32_t height;
    uint16_t max_value;
    uint64_t raster_size;
    uint32_t metadata_size;
    uint64_t payload_size;
    uint32_t crc32;
} NpbcContainerInfo;

NpbcStatus npbc_container_write(
    FILE *stream,
    const NpbcImage *image,
    NpbcCompressionAlgorithm algorithm
);
NpbcStatus npbc_container_read(
    FILE *stream,
    NpbcImage *image,
    NpbcContainerInfo *info
);
NpbcStatus npbc_container_inspect(
    FILE *stream,
    NpbcContainerInfo *info
);

#endif
