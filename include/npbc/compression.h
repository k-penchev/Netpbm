#ifndef NPBC_COMPRESSION_H
#define NPBC_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

#include "npbc/buffer.h"
#include "npbc/status.h"

typedef enum {
    NPBC_COMPRESSION_RLE = 1,
    NPBC_COMPRESSION_HUFFMAN = 2
} NpbcCompressionAlgorithm;

int npbc_compression_algorithm_valid(NpbcCompressionAlgorithm algorithm);
const char *npbc_compression_algorithm_name(
    NpbcCompressionAlgorithm algorithm
);
NpbcStatus npbc_compress(
    NpbcCompressionAlgorithm algorithm,
    const uint8_t *input,
    size_t input_size,
    NpbcBuffer *metadata,
    NpbcBuffer *payload
);
NpbcStatus npbc_decompress(
    NpbcCompressionAlgorithm algorithm,
    const uint8_t *metadata,
    size_t metadata_size,
    const uint8_t *payload,
    size_t payload_size,
    size_t expected_size,
    NpbcBuffer *output
);

#endif
