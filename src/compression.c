#include "npbc/compression.h"

#include "npbc/huffman.h"
#include "npbc/rle.h"

static int npbc_compression_buffer_is_empty(const NpbcBuffer *buffer) {
    return buffer != NULL
        && buffer->data == NULL
        && buffer->size == 0
        && buffer->capacity == 0;
}

int npbc_compression_algorithm_valid(NpbcCompressionAlgorithm algorithm) {
    return algorithm == NPBC_COMPRESSION_RLE
        || algorithm == NPBC_COMPRESSION_HUFFMAN;
}

const char *npbc_compression_algorithm_name(
    NpbcCompressionAlgorithm algorithm
) {
    switch (algorithm) {
        case NPBC_COMPRESSION_RLE:
            return "rle";
        case NPBC_COMPRESSION_HUFFMAN:
            return "huffman";
    }

    return "unknown";
}

NpbcStatus npbc_compress(
    NpbcCompressionAlgorithm algorithm,
    const uint8_t *input,
    size_t input_size,
    NpbcBuffer *metadata,
    NpbcBuffer *payload
) {
    if (
        !npbc_compression_algorithm_valid(algorithm)
        || (input == NULL && input_size != 0)
        || !npbc_compression_buffer_is_empty(metadata)
        || !npbc_compression_buffer_is_empty(payload)
        || metadata == payload
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    if (algorithm == NPBC_COMPRESSION_RLE) {
        return npbc_rle_encode(input, input_size, payload);
    }
    return npbc_huffman_encode(input, input_size, metadata, payload);
}

NpbcStatus npbc_decompress(
    NpbcCompressionAlgorithm algorithm,
    const uint8_t *metadata,
    size_t metadata_size,
    const uint8_t *payload,
    size_t payload_size,
    size_t expected_size,
    NpbcBuffer *output
) {
    if (!npbc_compression_algorithm_valid(algorithm)) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    if (algorithm == NPBC_COMPRESSION_RLE) {
        if (metadata_size != 0) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        return npbc_rle_decode(
            payload,
            payload_size,
            expected_size,
            output
        );
    }
    return npbc_huffman_decode(
        metadata,
        metadata_size,
        payload,
        payload_size,
        expected_size,
        output
    );
}
