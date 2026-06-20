#ifndef NPBC_HUFFMAN_H
#define NPBC_HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

#include "npbc/buffer.h"
#include "npbc/status.h"

#define NPBC_HUFFMAN_METADATA_SIZE ((size_t)264)
#define NPBC_HUFFMAN_MAX_CODE_LENGTH 63

NpbcStatus npbc_huffman_encode(
    const uint8_t *input,
    size_t input_size,
    NpbcBuffer *metadata,
    NpbcBuffer *payload
);
NpbcStatus npbc_huffman_validate_metadata(
    const uint8_t *metadata,
    size_t metadata_size,
    size_t expected_size,
    uint64_t *bit_count,
    size_t *payload_size
);
NpbcStatus npbc_huffman_decode(
    const uint8_t *metadata,
    size_t metadata_size,
    const uint8_t *payload,
    size_t payload_size,
    size_t expected_size,
    NpbcBuffer *output
);

#endif
