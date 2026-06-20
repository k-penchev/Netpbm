#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "npbc/buffer.h"
#include "npbc/rle.h"
#include "npbc/util.h"

static int npbc_test_rle_encode_bytes(
    const uint8_t *input,
    size_t input_size,
    const uint8_t *expected,
    size_t expected_size
) {
    NpbcBuffer encoded;

    npbc_buffer_init(&encoded);
    NPBC_ASSERT(
        npbc_rle_encode(input, input_size, &encoded) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(encoded.size == expected_size);
    NPBC_ASSERT_BYTES(encoded.data, expected, expected_size);
    npbc_buffer_destroy(&encoded);
    return 0;
}

static int npbc_test_rle_round_trip(
    const uint8_t *input,
    size_t input_size
) {
    NpbcBuffer encoded;
    NpbcBuffer decoded;

    npbc_buffer_init(&encoded);
    npbc_buffer_init(&decoded);
    NPBC_ASSERT(
        npbc_rle_encode(input, input_size, &encoded) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(
        npbc_rle_decode(
            encoded.data,
            encoded.size,
            input_size,
            &decoded
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(decoded.size == input_size);
    NPBC_ASSERT_BYTES(decoded.data, input, input_size);
    npbc_buffer_destroy(&decoded);
    npbc_buffer_destroy(&encoded);
    return 0;
}

static int npbc_test_rle_empty_and_small(void) {
    const uint8_t one[] = {0x42};
    const uint8_t two[] = {0x42, 0x42};
    const uint8_t three[] = {0x42, 0x42, 0x42};
    const uint8_t four[] = {0x42, 0x42, 0x42, 0x42};
    const uint8_t one_encoded[] = {0x00, 0x42};
    const uint8_t two_encoded[] = {0x01, 0x42, 0x42};
    const uint8_t three_encoded[] = {0x80, 0x42};
    const uint8_t four_encoded[] = {0x81, 0x42};
    NpbcBuffer output;

    npbc_buffer_init(&output);
    NPBC_ASSERT(npbc_rle_encode(NULL, 0, &output) == NPBC_STATUS_OK);
    NPBC_ASSERT(output.data == NULL);
    NPBC_ASSERT(output.size == 0);
    NPBC_ASSERT(output.capacity == 0);
    NPBC_ASSERT(npbc_rle_decode(NULL, 0, 0, &output) == NPBC_STATUS_OK);

    NPBC_ASSERT(
        npbc_test_rle_encode_bytes(
            one,
            sizeof(one),
            one_encoded,
            sizeof(one_encoded)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_encode_bytes(
            two,
            sizeof(two),
            two_encoded,
            sizeof(two_encoded)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_encode_bytes(
            three,
            sizeof(three),
            three_encoded,
            sizeof(three_encoded)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_encode_bytes(
            four,
            sizeof(four),
            four_encoded,
            sizeof(four_encoded)
        ) == 0
    );
    return 0;
}

static int npbc_test_rle_run_vector(
    size_t length,
    const uint8_t *expected_controls,
    size_t packet_count
) {
    uint8_t *input;
    NpbcBuffer encoded;
    size_t packet_index;

    input = malloc(length);
    NPBC_ASSERT(input != NULL);
    memset(input, 0x6d, length);
    npbc_buffer_init(&encoded);
    NPBC_ASSERT(
        npbc_rle_encode(input, length, &encoded) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(encoded.size == packet_count * 2);

    for (packet_index = 0; packet_index < packet_count; packet_index++) {
        NPBC_ASSERT(
            encoded.data[packet_index * 2] == expected_controls[packet_index]
        );
        NPBC_ASSERT(encoded.data[packet_index * 2 + 1] == 0x6d);
    }
    NPBC_ASSERT(npbc_test_rle_round_trip(input, length) == 0);

    npbc_buffer_destroy(&encoded);
    free(input);
    return 0;
}

static int npbc_test_rle_run_boundaries(void) {
    const uint8_t run_3[] = {0x80};
    const uint8_t run_127[] = {0xfc};
    const uint8_t run_128[] = {0xfd};
    const uint8_t run_129[] = {0xfe};
    const uint8_t run_130[] = {0xff};
    const uint8_t run_131[] = {0xfd, 0x80};
    const uint8_t run_132[] = {0xfe, 0x80};
    const uint8_t run_259[] = {0xff, 0xfe};
    const uint8_t run_260[] = {0xff, 0xff};
    const uint8_t run_261[] = {0xff, 0xfd, 0x80};
    const uint8_t run_262[] = {0xff, 0xfe, 0x80};

    NPBC_ASSERT(npbc_test_rle_run_vector(3, run_3, 1) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(127, run_127, 1) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(128, run_128, 1) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(129, run_129, 1) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(130, run_130, 1) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(131, run_131, 2) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(132, run_132, 2) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(259, run_259, 2) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(260, run_260, 2) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(261, run_261, 3) == 0);
    NPBC_ASSERT(npbc_test_rle_run_vector(262, run_262, 3) == 0);
    return 0;
}

static int npbc_test_rle_literals(void) {
    uint8_t input[256];
    NpbcBuffer encoded;
    size_t index;

    for (index = 0; index < sizeof(input); index++) {
        input[index] = (uint8_t)index;
    }

    npbc_buffer_init(&encoded);
    NPBC_ASSERT(npbc_rle_encode(input, 1, &encoded) == NPBC_STATUS_OK);
    NPBC_ASSERT(encoded.size == 2);
    NPBC_ASSERT(encoded.data[0] == 0);
    npbc_buffer_destroy(&encoded);

    npbc_buffer_init(&encoded);
    NPBC_ASSERT(npbc_rle_encode(input, 127, &encoded) == NPBC_STATUS_OK);
    NPBC_ASSERT(encoded.size == 128);
    NPBC_ASSERT(encoded.data[0] == 126);
    npbc_buffer_destroy(&encoded);

    npbc_buffer_init(&encoded);
    NPBC_ASSERT(npbc_rle_encode(input, 128, &encoded) == NPBC_STATUS_OK);
    NPBC_ASSERT(encoded.size == 129);
    NPBC_ASSERT(encoded.data[0] == 127);
    npbc_buffer_destroy(&encoded);

    npbc_buffer_init(&encoded);
    NPBC_ASSERT(npbc_rle_encode(input, 129, &encoded) == NPBC_STATUS_OK);
    NPBC_ASSERT(encoded.size == 131);
    NPBC_ASSERT(encoded.data[0] == 127);
    NPBC_ASSERT(encoded.data[129] == 0);
    NPBC_ASSERT(encoded.data[130] == 128);
    npbc_buffer_destroy(&encoded);

    NPBC_ASSERT(npbc_test_rle_round_trip(input, sizeof(input)) == 0);
    return 0;
}

static int npbc_test_rle_mixed_vector(void) {
    const uint8_t input[] = {1, 1, 2, 2, 2, 3, 4, 4};
    const uint8_t separated[] = {5, 5, 5, 6, 7, 8, 8, 8};
    const uint8_t expected[] = {
        0x01, 1, 1,
        0x80, 2,
        0x02, 3, 4, 4
    };
    const uint8_t separated_expected[] = {
        0x80, 5,
        0x01, 6, 7,
        0x80, 8
    };
    uint8_t expansion_input[128];
    NpbcBuffer encoded;
    size_t index;

    NPBC_ASSERT(
        npbc_test_rle_encode_bytes(
            input,
            sizeof(input),
            expected,
            sizeof(expected)
        ) == 0
    );
    NPBC_ASSERT(npbc_test_rle_round_trip(input, sizeof(input)) == 0);
    NPBC_ASSERT(
        npbc_test_rle_encode_bytes(
            separated,
            sizeof(separated),
            separated_expected,
            sizeof(separated_expected)
        ) == 0
    );

    for (index = 0; index < sizeof(expansion_input); index++) {
        expansion_input[index] = (uint8_t)(index & 1U);
    }
    npbc_buffer_init(&encoded);
    NPBC_ASSERT(
        npbc_rle_encode(
            expansion_input,
            sizeof(expansion_input),
            &encoded
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(encoded.size == sizeof(expansion_input) + 1);
    npbc_buffer_destroy(&encoded);
    return 0;
}

static int npbc_test_rle_packet_boundary(void) {
    uint8_t input[132];
    NpbcBuffer encoded;
    size_t index;

    for (index = 0; index < 127; index++) {
        input[index] = (uint8_t)index;
    }
    input[127] = 200;
    input[128] = 200;
    input[129] = 201;
    input[130] = 201;
    input[131] = 201;

    npbc_buffer_init(&encoded);
    NPBC_ASSERT(
        npbc_rle_encode(input, sizeof(input), &encoded) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(encoded.size == 133);
    NPBC_ASSERT(encoded.data[0] == 127);
    NPBC_ASSERT(encoded.data[129] == 0);
    NPBC_ASSERT(encoded.data[130] == 200);
    NPBC_ASSERT(encoded.data[131] == 0x80);
    NPBC_ASSERT(encoded.data[132] == 201);
    npbc_buffer_destroy(&encoded);

    NPBC_ASSERT(npbc_test_rle_round_trip(input, sizeof(input)) == 0);
    return 0;
}

static int npbc_test_rle_decode_failure(
    const uint8_t *input,
    size_t input_size,
    size_t expected_size,
    NpbcStatus expected_status
) {
    NpbcBuffer output;

    npbc_buffer_init(&output);
    NPBC_ASSERT(
        npbc_rle_decode(input, input_size, expected_size, &output)
            == expected_status
    );
    NPBC_ASSERT(output.data == NULL);
    NPBC_ASSERT(output.size == 0);
    NPBC_ASSERT(output.capacity == 0);
    return 0;
}

static int npbc_test_rle_malformed(void) {
    const uint8_t literal_missing[] = {0x02, 1, 2};
    const uint8_t repeated_missing[] = {0x80};
    const uint8_t overrun[] = {0x80, 1};
    const uint8_t underrun[] = {0x00, 1};
    const uint8_t extra[] = {0x00, 1, 0x00, 2};

    NPBC_ASSERT(
        npbc_test_rle_decode_failure(
            literal_missing,
            sizeof(literal_missing),
            3,
            NPBC_STATUS_TRUNCATED_INPUT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_decode_failure(
            repeated_missing,
            sizeof(repeated_missing),
            3,
            NPBC_STATUS_TRUNCATED_INPUT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_decode_failure(
            overrun,
            sizeof(overrun),
            2,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_decode_failure(
            underrun,
            sizeof(underrun),
            2,
            NPBC_STATUS_TRUNCATED_INPUT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_decode_failure(
            extra,
            sizeof(extra),
            1,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_rle_decode_failure(
            extra,
            sizeof(extra),
            0,
            NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    return 0;
}

static uint32_t npbc_test_rle_random(uint32_t *state) {
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int npbc_test_rle_random_round_trips(void) {
    uint8_t input[4096];
    uint32_t state;
    size_t iteration;

    state = UINT32_C(0x7a31d5c9);
    for (iteration = 0; iteration < 1000; iteration++) {
        size_t size = npbc_test_rle_random(&state) % (sizeof(input) + 1);
        size_t index;

        for (index = 0; index < size; index++) {
            uint32_t choice = npbc_test_rle_random(&state);

            if (index != 0 && choice % 5U == 0) {
                input[index] = input[index - 1];
            } else {
                input[index] = (uint8_t)choice;
            }
        }
        NPBC_ASSERT(npbc_test_rle_round_trip(input, size) == 0);
    }

    return 0;
}

static int npbc_test_rle_arguments(void) {
    const uint8_t input[] = {1};
    NpbcBuffer output;

    npbc_buffer_init(&output);
    NPBC_ASSERT(
        npbc_rle_encode(NULL, 1, &output) == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_rle_decode(NULL, 1, 1, &output) == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_rle_decode(
            input,
            sizeof(input),
            NPBC_MAX_RASTER_SIZE + 1,
            &output
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_rle_encode(
            input,
            NPBC_MAX_RASTER_SIZE + 1,
            &output
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_rle_encode(input, sizeof(input), NULL)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_rle_decode(input, sizeof(input), 1, NULL)
            == NPBC_STATUS_INVALID_ARGUMENT
    );

    output.data = malloc(1);
    NPBC_ASSERT(output.data != NULL);
    output.size = 0;
    output.capacity = 1;
    NPBC_ASSERT(
        npbc_rle_encode(input, sizeof(input), &output)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_rle_decode(input, sizeof(input), 1, &output)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    npbc_buffer_destroy(&output);
    return 0;
}

int npbc_test_rle(void) {
    NPBC_ASSERT(npbc_test_rle_empty_and_small() == 0);
    NPBC_ASSERT(npbc_test_rle_run_boundaries() == 0);
    NPBC_ASSERT(npbc_test_rle_literals() == 0);
    NPBC_ASSERT(npbc_test_rle_mixed_vector() == 0);
    NPBC_ASSERT(npbc_test_rle_packet_boundary() == 0);
    NPBC_ASSERT(npbc_test_rle_malformed() == 0);
    NPBC_ASSERT(npbc_test_rle_random_round_trips() == 0);
    NPBC_ASSERT(npbc_test_rle_arguments() == 0);
    return 0;
}
