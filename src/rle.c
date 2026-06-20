#include "npbc/rle.h"

#include <stdint.h>
#include <string.h>

#include "npbc/util.h"

static int npbc_rle_buffer_is_empty(const NpbcBuffer *buffer) {
    return buffer != NULL
        && buffer->data == NULL
        && buffer->size == 0
        && buffer->capacity == 0;
}

static size_t npbc_rle_run_length(
    const uint8_t *input,
    size_t input_size,
    size_t offset
) {
    size_t length;

    length = 1;
    while (
        length < input_size - offset
        && input[offset + length] == input[offset]
    ) {
        length++;
    }

    return length;
}

static NpbcStatus npbc_rle_append_repeated(
    NpbcBuffer *output,
    uint8_t value,
    size_t length
) {
    uint8_t packet[2];

    packet[0] = (uint8_t)(UINT8_C(0x80) | (uint8_t)(length - 3));
    packet[1] = value;
    return npbc_buffer_append(output, packet, sizeof(packet));
}

static NpbcStatus npbc_rle_encode_run(
    NpbcBuffer *output,
    uint8_t value,
    size_t length
) {
    NpbcStatus status;

    while (length > 130) {
        size_t packet_length;

        if (length - 130 == 1) {
            packet_length = 128;
        } else if (length - 130 == 2) {
            packet_length = 129;
        } else {
            packet_length = 130;
        }

        status = npbc_rle_append_repeated(output, value, packet_length);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        length -= packet_length;
    }

    return npbc_rle_append_repeated(output, value, length);
}

static NpbcStatus npbc_rle_encode_literal(
    NpbcBuffer *output,
    const uint8_t *input,
    size_t length
) {
    uint8_t control;
    NpbcStatus status;

    control = (uint8_t)(length - 1);
    status = npbc_buffer_append_byte(output, control);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    return npbc_buffer_append(output, input, length);
}

NpbcStatus npbc_rle_encode(
    const uint8_t *input,
    size_t input_size,
    NpbcBuffer *output
) {
    NpbcBuffer encoded;
    size_t offset;
    NpbcStatus status;

    if (
        (input == NULL && input_size != 0)
        || !npbc_rle_buffer_is_empty(output)
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (input_size > NPBC_MAX_RASTER_SIZE) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    npbc_buffer_init(&encoded);
    offset = 0;
    while (offset < input_size) {
        size_t run_length = npbc_rle_run_length(input, input_size, offset);

        if (run_length >= 3) {
            status = npbc_rle_encode_run(
                &encoded,
                input[offset],
                run_length
            );
            if (status != NPBC_STATUS_OK) {
                npbc_buffer_destroy(&encoded);
                return status;
            }
            offset += run_length;
        } else {
            size_t literal_start = offset;

            while (offset < input_size && offset - literal_start < 128) {
                run_length = npbc_rle_run_length(input, input_size, offset);
                if (run_length >= 3) {
                    break;
                }
                offset++;
            }

            status = npbc_rle_encode_literal(
                &encoded,
                input + literal_start,
                offset - literal_start
            );
            if (status != NPBC_STATUS_OK) {
                npbc_buffer_destroy(&encoded);
                return status;
            }
        }
    }

    *output = encoded;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_rle_decode_literal(
    const uint8_t *input,
    size_t input_size,
    size_t *input_offset,
    size_t length,
    NpbcBuffer *output,
    size_t expected_size
) {
    NpbcStatus status;

    if (length > input_size - *input_offset) {
        return NPBC_STATUS_TRUNCATED_INPUT;
    }
    if (length > expected_size - output->size) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    status = npbc_buffer_append(output, input + *input_offset, length);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    *input_offset += length;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_rle_decode_repeated(
    const uint8_t *input,
    size_t input_size,
    size_t *input_offset,
    size_t length,
    NpbcBuffer *output,
    size_t expected_size
) {
    uint8_t value;

    if (*input_offset >= input_size) {
        return NPBC_STATUS_TRUNCATED_INPUT;
    }
    if (length > expected_size - output->size) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    value = input[*input_offset];
    *input_offset += 1;
    memset(output->data + output->size, value, length);
    output->size += length;
    return NPBC_STATUS_OK;
}

NpbcStatus npbc_rle_decode(
    const uint8_t *input,
    size_t input_size,
    size_t expected_size,
    NpbcBuffer *output
) {
    NpbcBuffer decoded;
    size_t input_offset;
    NpbcStatus status;

    if (
        (input == NULL && input_size != 0)
        || !npbc_rle_buffer_is_empty(output)
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (expected_size > NPBC_MAX_RASTER_SIZE) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }
    if (expected_size == 0) {
        if (input_size != 0) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        return NPBC_STATUS_OK;
    }
    if (input_size == 0) {
        return NPBC_STATUS_TRUNCATED_INPUT;
    }

    npbc_buffer_init(&decoded);
    status = npbc_buffer_reserve(&decoded, expected_size);
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    input_offset = 0;
    while (input_offset < input_size && decoded.size < expected_size) {
        uint8_t control = input[input_offset];
        size_t length;

        input_offset++;
        if ((control & UINT8_C(0x80)) == 0) {
            length = (size_t)control + 1;
            status = npbc_rle_decode_literal(
                input,
                input_size,
                &input_offset,
                length,
                &decoded,
                expected_size
            );
        } else {
            length = (size_t)(control & UINT8_C(0x7f)) + 3;
            status = npbc_rle_decode_repeated(
                input,
                input_size,
                &input_offset,
                length,
                &decoded,
                expected_size
            );
        }

        if (status != NPBC_STATUS_OK) {
            npbc_buffer_destroy(&decoded);
            return status;
        }
    }

    if (decoded.size != expected_size) {
        npbc_buffer_destroy(&decoded);
        return NPBC_STATUS_TRUNCATED_INPUT;
    }
    if (input_offset != input_size) {
        npbc_buffer_destroy(&decoded);
        return NPBC_STATUS_INVALID_FORMAT;
    }

    *output = decoded;
    return NPBC_STATUS_OK;
}
