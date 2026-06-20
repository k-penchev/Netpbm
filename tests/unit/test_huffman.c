#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "npbc/buffer.h"
#include "npbc/huffman.h"
#include "npbc/util.h"

static void npbc_test_huffman_metadata(
    uint8_t metadata[NPBC_HUFFMAN_METADATA_SIZE],
    uint64_t bit_count
) {
    memset(metadata, 0, NPBC_HUFFMAN_METADATA_SIZE);
    npbc_write_u64_be(metadata, bit_count);
}

static int npbc_test_huffman_round_trip(
    const uint8_t *input,
    size_t input_size
) {
    NpbcBuffer metadata;
    NpbcBuffer payload;
    NpbcBuffer decoded;

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    npbc_buffer_init(&decoded);
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            input_size,
            &metadata,
            &payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(metadata.size == NPBC_HUFFMAN_METADATA_SIZE);
    NPBC_ASSERT(
        npbc_huffman_decode(
            metadata.data,
            metadata.size,
            payload.data,
            payload.size,
            input_size,
            &decoded
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(decoded.size == input_size);
    NPBC_ASSERT_BYTES(decoded.data, input, input_size);

    npbc_buffer_destroy(&decoded);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);
    return 0;
}

static int npbc_test_huffman_empty(void) {
    NpbcBuffer metadata;
    NpbcBuffer payload;
    NpbcBuffer decoded;
    size_t index;

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    npbc_buffer_init(&decoded);
    NPBC_ASSERT(
        npbc_huffman_encode(NULL, 0, &metadata, &payload)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(metadata.size == NPBC_HUFFMAN_METADATA_SIZE);
    NPBC_ASSERT(npbc_read_u64_be(metadata.data) == 0);
    for (index = 8; index < metadata.size; index++) {
        NPBC_ASSERT(metadata.data[index] == 0);
    }
    NPBC_ASSERT(payload.data == NULL);
    NPBC_ASSERT(payload.size == 0);
    NPBC_ASSERT(
        npbc_huffman_decode(
            metadata.data,
            metadata.size,
            NULL,
            0,
            0,
            &decoded
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(decoded.data == NULL);
    NPBC_ASSERT(decoded.size == 0);

    npbc_buffer_destroy(&decoded);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);
    return 0;
}

static int npbc_test_huffman_single_symbol(void) {
    uint8_t input[10];
    uint8_t one[] = {0};
    uint8_t eight[8];
    uint8_t nine[9];
    NpbcBuffer metadata;
    NpbcBuffer payload;
    size_t index;

    memset(input, 0xa5, sizeof(input));
    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            sizeof(input),
            &metadata,
            &payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(npbc_read_u64_be(metadata.data) == 10);
    for (index = 0; index < 256; index++) {
        NPBC_ASSERT(
            metadata.data[8 + index] == (index == 0xa5 ? 1 : 0)
        );
    }
    NPBC_ASSERT(payload.size == 2);
    NPBC_ASSERT(payload.data[0] == 0);
    NPBC_ASSERT(payload.data[1] == 0);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);

    NPBC_ASSERT(npbc_test_huffman_round_trip(input, sizeof(input)) == 0);
    memset(eight, 0x7f, sizeof(eight));
    memset(nine, 0xff, sizeof(nine));
    NPBC_ASSERT(npbc_test_huffman_round_trip(one, sizeof(one)) == 0);
    NPBC_ASSERT(npbc_test_huffman_round_trip(eight, sizeof(eight)) == 0);
    NPBC_ASSERT(npbc_test_huffman_round_trip(nine, sizeof(nine)) == 0);
    return 0;
}

static int npbc_test_huffman_fixed_vectors(void) {
    const uint8_t two_symbols[] = {0, 1, 0, 1};
    const uint8_t three_symbols[] = {0, 1, 2};
    NpbcBuffer metadata;
    NpbcBuffer payload;

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    NPBC_ASSERT(
        npbc_huffman_encode(
            two_symbols,
            sizeof(two_symbols),
            &metadata,
            &payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(npbc_read_u64_be(metadata.data) == 4);
    NPBC_ASSERT(metadata.data[8] == 1);
    NPBC_ASSERT(metadata.data[9] == 1);
    NPBC_ASSERT(payload.size == 1);
    NPBC_ASSERT(payload.data[0] == 0x50);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    NPBC_ASSERT(
        npbc_huffman_encode(
            three_symbols,
            sizeof(three_symbols),
            &metadata,
            &payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(npbc_read_u64_be(metadata.data) == 5);
    NPBC_ASSERT(metadata.data[8] == 2);
    NPBC_ASSERT(metadata.data[9] == 2);
    NPBC_ASSERT(metadata.data[10] == 1);
    NPBC_ASSERT(payload.size == 1);
    NPBC_ASSERT(payload.data[0] == 0xb0);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);

    NPBC_ASSERT(
        npbc_test_huffman_round_trip(
            two_symbols,
            sizeof(two_symbols)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_huffman_round_trip(
            three_symbols,
            sizeof(three_symbols)
        ) == 0
    );
    return 0;
}

static int npbc_test_huffman_all_symbols(void) {
    uint8_t input[256];
    NpbcBuffer metadata;
    NpbcBuffer payload;
    size_t index;

    for (index = 0; index < sizeof(input); index++) {
        input[index] = (uint8_t)index;
    }

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            sizeof(input),
            &metadata,
            &payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(npbc_read_u64_be(metadata.data) == 2048);
    for (index = 0; index < 256; index++) {
        NPBC_ASSERT(metadata.data[8 + index] == 8);
    }
    NPBC_ASSERT(payload.size == sizeof(input));
    NPBC_ASSERT_BYTES(payload.data, input, sizeof(input));
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);

    NPBC_ASSERT(npbc_test_huffman_round_trip(input, sizeof(input)) == 0);
    return 0;
}

static int npbc_test_huffman_skewed(void) {
    uint8_t input[2048];
    size_t index;

    for (index = 0; index < sizeof(input); index++) {
        if (index < 1600) {
            input[index] = 0;
        } else if (index < 1900) {
            input[index] = 1;
        } else if (index < 2020) {
            input[index] = 2;
        } else {
            input[index] = (uint8_t)(3 + index % 13);
        }
    }

    NPBC_ASSERT(npbc_test_huffman_round_trip(input, sizeof(input)) == 0);
    return 0;
}

static int npbc_test_huffman_maximum_code_length(void) {
    uint8_t metadata[NPBC_HUFFMAN_METADATA_SIZE];
    const uint8_t payload[] = {0};
    NpbcBuffer output;
    size_t symbol;

    npbc_test_huffman_metadata(metadata, 1);
    for (symbol = 0; symbol < 62; symbol++) {
        metadata[8 + symbol] = (uint8_t)(symbol + 1);
    }
    metadata[8 + 62] = 63;
    metadata[8 + 63] = 63;

    npbc_buffer_init(&output);
    NPBC_ASSERT(
        npbc_huffman_decode(
            metadata,
            sizeof(metadata),
            payload,
            sizeof(payload),
            1,
            &output
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(output.size == 1);
    NPBC_ASSERT(output.data[0] == 0);
    npbc_buffer_destroy(&output);
    return 0;
}

static int npbc_test_huffman_determinism(void) {
    const uint8_t input[] = {
        7, 6, 5, 4, 3, 2, 1, 0,
        0, 1, 2, 3, 4, 5, 6, 7
    };
    NpbcBuffer first_metadata;
    NpbcBuffer first_payload;
    NpbcBuffer second_metadata;
    NpbcBuffer second_payload;

    npbc_buffer_init(&first_metadata);
    npbc_buffer_init(&first_payload);
    npbc_buffer_init(&second_metadata);
    npbc_buffer_init(&second_payload);
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            sizeof(input),
            &first_metadata,
            &first_payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            sizeof(input),
            &second_metadata,
            &second_payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(first_metadata.size == second_metadata.size);
    NPBC_ASSERT(first_payload.size == second_payload.size);
    NPBC_ASSERT_BYTES(
        first_metadata.data,
        second_metadata.data,
        first_metadata.size
    );
    NPBC_ASSERT_BYTES(
        first_payload.data,
        second_payload.data,
        first_payload.size
    );

    npbc_buffer_destroy(&second_payload);
    npbc_buffer_destroy(&second_metadata);
    npbc_buffer_destroy(&first_payload);
    npbc_buffer_destroy(&first_metadata);
    return 0;
}

static int npbc_test_huffman_decode_failure(
    const uint8_t *metadata,
    size_t metadata_size,
    const uint8_t *payload,
    size_t payload_size,
    size_t expected_size,
    NpbcStatus expected_status
) {
    NpbcBuffer output;

    npbc_buffer_init(&output);
    NPBC_ASSERT(
        npbc_huffman_decode(
            metadata,
            metadata_size,
            payload,
            payload_size,
            expected_size,
            &output
        ) == expected_status
    );
    NPBC_ASSERT(output.data == NULL);
    NPBC_ASSERT(output.size == 0);
    NPBC_ASSERT(output.capacity == 0);
    return 0;
}

static int npbc_test_huffman_invalid_tables(void) {
    uint8_t metadata[NPBC_HUFFMAN_METADATA_SIZE];
    const uint8_t zero_payload[] = {0};

    npbc_test_huffman_metadata(metadata, 1);
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            zero_payload,
            sizeof(zero_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 0);
    metadata[8] = 1;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            NULL,
            0,
            0,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 1);
    metadata[8] = 2;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            zero_payload,
            sizeof(zero_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 1);
    metadata[8] = 64;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            zero_payload,
            sizeof(zero_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 2);
    metadata[8] = 1;
    metadata[9] = 1;
    metadata[10] = 1;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            zero_payload,
            sizeof(zero_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 2);
    metadata[8] = 2;
    metadata[9] = 2;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            zero_payload,
            sizeof(zero_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    return 0;
}

static int npbc_test_huffman_invalid_streams(void) {
    uint8_t metadata[NPBC_HUFFMAN_METADATA_SIZE];
    const uint8_t zero_payload[] = {0};
    const uint8_t long_payload[] = {0, 0};
    const uint8_t one_payload[] = {0x80};
    const uint8_t bad_padding[] = {0x01};

    npbc_test_huffman_metadata(metadata, 8);
    metadata[8] = 1;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            NULL,
            0,
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            long_payload,
            sizeof(long_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 1);
    metadata[8] = 1;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            bad_padding,
            sizeof(bad_padding),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            one_payload,
            sizeof(one_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            zero_payload,
            sizeof(zero_payload),
            2,
            NPBC_STATUS_TRUNCATED_INPUT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 2);
    metadata[8] = 1;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            sizeof(metadata),
            zero_payload,
            sizeof(zero_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    npbc_test_huffman_metadata(metadata, 1);
    metadata[8] = 1;
    NPBC_ASSERT(
        npbc_test_huffman_decode_failure(
            metadata,
            NPBC_HUFFMAN_METADATA_SIZE - 1,
            zero_payload,
            sizeof(zero_payload),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    return 0;
}

static uint32_t npbc_test_huffman_random(uint32_t *state) {
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int npbc_test_huffman_random_round_trips(void) {
    uint8_t input[4096];
    uint32_t state;
    size_t iteration;

    state = UINT32_C(0xc5917a3d);
    for (iteration = 0; iteration < 500; iteration++) {
        size_t size = npbc_test_huffman_random(&state) % (sizeof(input) + 1);
        size_t alphabet = 1 + npbc_test_huffman_random(&state) % 256;
        size_t index;

        for (index = 0; index < size; index++) {
            input[index] = (uint8_t)(
                npbc_test_huffman_random(&state) % alphabet
            );
        }
        NPBC_ASSERT(npbc_test_huffman_round_trip(input, size) == 0);
    }

    return 0;
}

static int npbc_test_huffman_arguments(void) {
    const uint8_t input[] = {1};
    uint8_t metadata_bytes[NPBC_HUFFMAN_METADATA_SIZE];
    NpbcBuffer metadata;
    NpbcBuffer payload;
    NpbcBuffer output;

    npbc_test_huffman_metadata(metadata_bytes, 0);
    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    npbc_buffer_init(&output);

    NPBC_ASSERT(
        npbc_huffman_encode(NULL, 1, &metadata, &payload)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            NPBC_MAX_RASTER_SIZE + 1,
            &metadata,
            &payload
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_huffman_encode(input, sizeof(input), NULL, &payload)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_huffman_encode(input, sizeof(input), &metadata, &metadata)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    metadata.data = malloc(1);
    NPBC_ASSERT(metadata.data != NULL);
    metadata.size = 0;
    metadata.capacity = 1;
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            sizeof(input),
            &metadata,
            &payload
        ) == NPBC_STATUS_INVALID_ARGUMENT
    );
    npbc_buffer_destroy(&metadata);
    payload.data = malloc(1);
    NPBC_ASSERT(payload.data != NULL);
    payload.size = 0;
    payload.capacity = 1;
    NPBC_ASSERT(
        npbc_huffman_encode(
            input,
            sizeof(input),
            &metadata,
            &payload
        ) == NPBC_STATUS_INVALID_ARGUMENT
    );
    npbc_buffer_destroy(&payload);
    NPBC_ASSERT(
        npbc_huffman_decode(
            metadata_bytes,
            sizeof(metadata_bytes),
            NULL,
            0,
            NPBC_MAX_RASTER_SIZE + 1,
            &output
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_huffman_decode(
            NULL,
            sizeof(metadata_bytes),
            NULL,
            0,
            0,
            &output
        ) == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_huffman_decode(
            metadata_bytes,
            sizeof(metadata_bytes),
            input,
            sizeof(input),
            0,
            NULL
        ) == NPBC_STATUS_INVALID_ARGUMENT
    );

    output.data = malloc(1);
    NPBC_ASSERT(output.data != NULL);
    output.size = 0;
    output.capacity = 1;
    NPBC_ASSERT(
        npbc_huffman_decode(
            metadata_bytes,
            sizeof(metadata_bytes),
            NULL,
            0,
            0,
            &output
        ) == NPBC_STATUS_INVALID_ARGUMENT
    );
    npbc_buffer_destroy(&output);
    return 0;
}

int npbc_test_huffman(void) {
    NPBC_ASSERT(npbc_test_huffman_empty() == 0);
    NPBC_ASSERT(npbc_test_huffman_single_symbol() == 0);
    NPBC_ASSERT(npbc_test_huffman_fixed_vectors() == 0);
    NPBC_ASSERT(npbc_test_huffman_all_symbols() == 0);
    NPBC_ASSERT(npbc_test_huffman_skewed() == 0);
    NPBC_ASSERT(npbc_test_huffman_maximum_code_length() == 0);
    NPBC_ASSERT(npbc_test_huffman_determinism() == 0);
    NPBC_ASSERT(npbc_test_huffman_invalid_tables() == 0);
    NPBC_ASSERT(npbc_test_huffman_invalid_streams() == 0);
    NPBC_ASSERT(npbc_test_huffman_random_round_trips() == 0);
    NPBC_ASSERT(npbc_test_huffman_arguments() == 0);
    return 0;
}
