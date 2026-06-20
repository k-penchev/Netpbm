#ifndef NPBC_RLE_H
#define NPBC_RLE_H

#include <stddef.h>
#include <stdint.h>

#include "npbc/buffer.h"
#include "npbc/status.h"

NpbcStatus npbc_rle_encode(
    const uint8_t *input,
    size_t input_size,
    NpbcBuffer *output
);
NpbcStatus npbc_rle_decode(
    const uint8_t *input,
    size_t input_size,
    size_t expected_size,
    NpbcBuffer *output
);

#endif
