#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npbc/buffer.h"
#include "npbc/compression.h"
#include "npbc/container.h"
#include "npbc/huffman.h"
#include "npbc/image.h"
#include "npbc/netpbm.h"
#include "npbc/rle.h"
#include "npbc/status.h"
#include "npbc/util.h"

#define NPBC_ROBUSTNESS_ITERATIONS 4000

static uint32_t npbc_test_robustness_random(uint32_t *state) {
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int npbc_test_robustness_status_known(NpbcStatus status) {
    return status >= NPBC_STATUS_OK
        && status <= NPBC_STATUS_INTERNAL_ERROR;
}

static int npbc_test_robustness_image_empty(const NpbcImage *image) {
    return image->kind == 0
        && image->width == 0
        && image->height == 0
        && image->channels == 0
        && image->max_value == 0
        && image->raster == NULL
        && image->raster_size == 0;
}

static FILE *npbc_test_robustness_stream(
    const uint8_t *data,
    size_t size
) {
    FILE *stream = tmpfile();

    if (stream == NULL) {
        return NULL;
    }
    if (size != 0 && fwrite(data, 1, size, stream) != size) {
        fclose(stream);
        return NULL;
    }
    if (fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return NULL;
    }
    return stream;
}

static int npbc_test_robustness_rle_random(void) {
    uint8_t input[128];
    uint32_t state = UINT32_C(0x1f2e3d4c);
    size_t iteration;

    for (iteration = 0; iteration < NPBC_ROBUSTNESS_ITERATIONS; iteration++) {
        NpbcBuffer output;
        size_t input_size =
            npbc_test_robustness_random(&state) % (sizeof(input) + 1);
        size_t expected_size =
            npbc_test_robustness_random(&state) % (sizeof(input) + 1);
        size_t index;
        NpbcStatus status;

        for (index = 0; index < input_size; index++) {
            input[index] = (uint8_t)npbc_test_robustness_random(&state);
        }

        npbc_buffer_init(&output);
        status = npbc_rle_decode(
            input,
            input_size,
            expected_size,
            &output
        );
        NPBC_ASSERT(npbc_test_robustness_status_known(status));
        if (status == NPBC_STATUS_OK) {
            NPBC_ASSERT(output.size == expected_size);
        } else {
            NPBC_ASSERT(output.data == NULL);
            NPBC_ASSERT(output.size == 0);
            NPBC_ASSERT(output.capacity == 0);
        }
        npbc_buffer_destroy(&output);
    }

    return 0;
}

static void npbc_test_robustness_huffman_metadata(
    uint8_t metadata[NPBC_HUFFMAN_METADATA_SIZE],
    uint32_t *state,
    size_t expected_size
) {
    size_t index;
    uint64_t bit_count;

    memset(metadata, 0, NPBC_HUFFMAN_METADATA_SIZE);
    bit_count = npbc_test_robustness_random(state) % 513U;
    npbc_write_u64_be(metadata, bit_count);

    if (expected_size != 0 && npbc_test_robustness_random(state) % 4U == 0) {
        metadata[8] = 1;
        metadata[9] = 1;
        return;
    }

    for (index = 8; index < NPBC_HUFFMAN_METADATA_SIZE; index++) {
        metadata[index] = (uint8_t)(
            npbc_test_robustness_random(state) % 66U
        );
    }
}

static int npbc_test_robustness_huffman_random(void) {
    uint8_t metadata[NPBC_HUFFMAN_METADATA_SIZE];
    uint8_t payload[65];
    uint32_t state = UINT32_C(0xa5b4c3d2);
    size_t iteration;

    for (iteration = 0; iteration < NPBC_ROBUSTNESS_ITERATIONS; iteration++) {
        NpbcBuffer output;
        size_t expected_size = npbc_test_robustness_random(&state) % 65U;
        size_t payload_size = npbc_test_robustness_random(&state) % 66U;
        size_t index;
        NpbcStatus status;

        npbc_test_robustness_huffman_metadata(
            metadata,
            &state,
            expected_size
        );
        for (index = 0; index < payload_size; index++) {
            payload[index] = (uint8_t)npbc_test_robustness_random(&state);
        }

        npbc_buffer_init(&output);
        status = npbc_huffman_decode(
            metadata,
            sizeof(metadata),
            payload,
            payload_size,
            expected_size,
            &output
        );
        NPBC_ASSERT(npbc_test_robustness_status_known(status));
        if (status == NPBC_STATUS_OK) {
            NPBC_ASSERT(output.size == expected_size);
        } else {
            NPBC_ASSERT(output.data == NULL);
            NPBC_ASSERT(output.size == 0);
            NPBC_ASSERT(output.capacity == 0);
        }
        npbc_buffer_destroy(&output);
    }

    return 0;
}

static int npbc_test_robustness_make_container(
    NpbcCompressionAlgorithm algorithm,
    uint8_t **data,
    size_t *size
) {
    static const uint8_t raster[] = {
        0, 1, 2, 3, 3, 3, 4, 5,
        6, 6, 6, 6, 7, 8, 9, 10
    };
    NpbcImage image;
    FILE *stream;
    long length;

    npbc_image_init(&image);
    image.kind = NPBC_IMAGE_GRAYSCALE;
    image.width = sizeof(raster);
    image.height = 1;
    image.channels = 1;
    image.max_value = 255;
    image.raster_size = sizeof(raster);
    image.raster = malloc(sizeof(raster));
    NPBC_ASSERT(image.raster != NULL);
    memcpy(image.raster, raster, sizeof(raster));

    stream = tmpfile();
    NPBC_ASSERT(stream != NULL);
    NPBC_ASSERT(
        npbc_container_write(stream, &image, algorithm) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(fflush(stream) == 0);
    NPBC_ASSERT(fseek(stream, 0, SEEK_END) == 0);
    length = ftell(stream);
    NPBC_ASSERT(length > 0);
    NPBC_ASSERT(fseek(stream, 0, SEEK_SET) == 0);

    *size = (size_t)length;
    *data = malloc(*size);
    NPBC_ASSERT(*data != NULL);
    NPBC_ASSERT(fread(*data, 1, *size, stream) == *size);
    NPBC_ASSERT(fclose(stream) == 0);
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_robustness_container_attempt(
    const uint8_t *data,
    size_t size
) {
    NpbcContainerInfo info;
    NpbcImage image;
    FILE *stream;
    NpbcStatus status;

    stream = npbc_test_robustness_stream(data, size);
    NPBC_ASSERT(stream != NULL);
    status = npbc_container_inspect(stream, &info);
    NPBC_ASSERT(npbc_test_robustness_status_known(status));
    NPBC_ASSERT(fclose(stream) == 0);

    stream = npbc_test_robustness_stream(data, size);
    NPBC_ASSERT(stream != NULL);
    npbc_image_init(&image);
    status = npbc_container_read(stream, &image, &info);
    NPBC_ASSERT(npbc_test_robustness_status_known(status));
    if (status == NPBC_STATUS_OK) {
        NPBC_ASSERT(image.raster != NULL);
    } else {
        NPBC_ASSERT(npbc_test_robustness_image_empty(&image));
    }
    npbc_image_destroy(&image);
    NPBC_ASSERT(fclose(stream) == 0);
    return 0;
}

static int npbc_test_robustness_container_truncations(
    const uint8_t *data,
    size_t size
) {
    size_t length;

    for (length = 0; length < size; length++) {
        NpbcImage image;
        FILE *stream = npbc_test_robustness_stream(data, length);
        NpbcStatus status;

        NPBC_ASSERT(stream != NULL);
        npbc_image_init(&image);
        status = npbc_container_read(stream, &image, NULL);
        NPBC_ASSERT(status != NPBC_STATUS_OK);
        NPBC_ASSERT(npbc_test_robustness_image_empty(&image));
        NPBC_ASSERT(fclose(stream) == 0);
    }

    return 0;
}

static int npbc_test_robustness_container_header_bits(
    const uint8_t *data,
    size_t size
) {
    uint8_t *mutated;
    size_t offset;

    NPBC_ASSERT(size >= NPBC_CONTAINER_HEADER_SIZE);
    mutated = malloc(size);
    NPBC_ASSERT(mutated != NULL);

    for (offset = 0; offset < NPBC_CONTAINER_HEADER_SIZE; offset++) {
        unsigned int bit;

        for (bit = 0; bit < 8; bit++) {
            memcpy(mutated, data, size);
            mutated[offset] ^= (uint8_t)(UINT8_C(1) << bit);
            NPBC_ASSERT(
                npbc_test_robustness_container_attempt(
                    mutated,
                    size
                ) == 0
            );
        }
    }

    free(mutated);
    return 0;
}

static int npbc_test_robustness_container_random(
    const uint8_t *data,
    size_t size,
    uint32_t seed
) {
    uint8_t *mutated;
    uint32_t state = seed;
    size_t iteration;

    mutated = malloc(size);
    NPBC_ASSERT(mutated != NULL);

    for (iteration = 0; iteration < NPBC_ROBUSTNESS_ITERATIONS; iteration++) {
        size_t mutation_count;
        size_t mutation;
        size_t used_size;

        memcpy(mutated, data, size);
        mutation_count = 1 + npbc_test_robustness_random(&state) % 4U;
        for (mutation = 0; mutation < mutation_count; mutation++) {
            size_t offset = npbc_test_robustness_random(&state) % size;
            uint8_t value = (uint8_t)npbc_test_robustness_random(&state);

            mutated[offset] ^= (uint8_t)(value | UINT8_C(1));
        }
        used_size = npbc_test_robustness_random(&state) % 8U == 0
            ? npbc_test_robustness_random(&state) % size
            : size;
        NPBC_ASSERT(
            npbc_test_robustness_container_attempt(
                mutated,
                used_size
            ) == 0
        );
    }

    free(mutated);
    return 0;
}

static int npbc_test_robustness_containers(void) {
    static const NpbcCompressionAlgorithm algorithms[] = {
        NPBC_COMPRESSION_RLE,
        NPBC_COMPRESSION_HUFFMAN
    };
    size_t index;

    for (index = 0; index < sizeof(algorithms) / sizeof(algorithms[0]); index++) {
        uint8_t *data = NULL;
        size_t size = 0;

        NPBC_ASSERT(
            npbc_test_robustness_make_container(
                algorithms[index],
                &data,
                &size
            ) == 0
        );
        NPBC_ASSERT(
            npbc_test_robustness_container_truncations(data, size) == 0
        );
        NPBC_ASSERT(
            npbc_test_robustness_container_header_bits(data, size) == 0
        );
        NPBC_ASSERT(
            npbc_test_robustness_container_random(
                data,
                size,
                UINT32_C(0x91e10da5) + (uint32_t)index
            ) == 0
        );
        free(data);
    }

    return 0;
}

static int npbc_test_robustness_netpbm_attempt(
    const uint8_t *data,
    size_t size
) {
    NpbcImage image;
    FILE *stream = npbc_test_robustness_stream(data, size);
    NpbcStatus status;

    NPBC_ASSERT(stream != NULL);
    npbc_image_init(&image);
    status = npbc_netpbm_read(stream, &image);
    NPBC_ASSERT(npbc_test_robustness_status_known(status));
    if (status != NPBC_STATUS_OK) {
        NPBC_ASSERT(npbc_test_robustness_image_empty(&image));
    }
    npbc_image_destroy(&image);
    NPBC_ASSERT(fclose(stream) == 0);
    return 0;
}

static int npbc_test_robustness_netpbm_corpus(void) {
    static const char *paths[] = {
        "tests/fixtures/malformed/netpbm-negative.pgm",
        "tests/fixtures/malformed/netpbm-overflow.pgm",
        "tests/fixtures/malformed/netpbm-trailing.ppm",
        "tests/fixtures/malformed/netpbm-truncated.pgm"
    };
    size_t index;

    for (index = 0; index < sizeof(paths) / sizeof(paths[0]); index++) {
        NpbcImage image;
        FILE *stream = fopen(paths[index], "rb");

        NPBC_ASSERT(stream != NULL);
        npbc_image_init(&image);
        NPBC_ASSERT(npbc_netpbm_read(stream, &image) != NPBC_STATUS_OK);
        NPBC_ASSERT(npbc_test_robustness_image_empty(&image));
        NPBC_ASSERT(fclose(stream) == 0);
    }

    return 0;
}

static int npbc_test_robustness_netpbm_truncations(void) {
    static const uint8_t p1[] = "P1\n1 1\n0";
    static const uint8_t p2[] = "P2\n1 1\n1\n0";
    static const uint8_t p3[] = "P3\n1 1\n1\n0 0 0";
    static const uint8_t p4[] = {'P', '4', '\n', '1', ' ', '1', '\n', 0x80};
    static const uint8_t p5[] = {
        'P', '5', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', '#'
    };
    static const uint8_t p6[] = {
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 1, 2, 3
    };
    static const struct {
        const uint8_t *data;
        size_t size;
    } inputs[] = {
        {p1, sizeof(p1) - 1},
        {p2, sizeof(p2) - 1},
        {p3, sizeof(p3) - 1},
        {p4, sizeof(p4)},
        {p5, sizeof(p5)},
        {p6, sizeof(p6)}
    };
    size_t input_index;

    for (
        input_index = 0;
        input_index < sizeof(inputs) / sizeof(inputs[0]);
        input_index++
    ) {
        size_t length;

        for (length = 0; length < inputs[input_index].size; length++) {
            NpbcImage image;
            FILE *stream = npbc_test_robustness_stream(
                inputs[input_index].data,
                length
            );

            NPBC_ASSERT(stream != NULL);
            npbc_image_init(&image);
            NPBC_ASSERT(npbc_netpbm_read(stream, &image) != NPBC_STATUS_OK);
            NPBC_ASSERT(npbc_test_robustness_image_empty(&image));
            NPBC_ASSERT(fclose(stream) == 0);
        }
        NPBC_ASSERT(
            npbc_test_robustness_netpbm_attempt(
                inputs[input_index].data,
                inputs[input_index].size
            ) == 0
        );
    }

    return 0;
}

static int npbc_test_robustness_netpbm_random(void) {
    static const uint8_t p1[] = "P1\n2 2\n0 1 1 0\n";
    static const uint8_t p2[] = "P2\n2 1\n15\n0 15\n";
    static const uint8_t p3[] = "P3\n1 1\n255\n1 2 3\n";
    static const uint8_t p4[] = {'P', '4', '\n', '8', ' ', '1', '\n', 0xa5};
    static const uint8_t p5[] = {
        'P', '5', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', '#'
    };
    static const uint8_t p6[] = {
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 1, 2, 3
    };
    static const struct {
        const uint8_t *data;
        size_t size;
    } inputs[] = {
        {p1, sizeof(p1) - 1},
        {p2, sizeof(p2) - 1},
        {p3, sizeof(p3) - 1},
        {p4, sizeof(p4)},
        {p5, sizeof(p5)},
        {p6, sizeof(p6)}
    };
    uint8_t mutated[sizeof(p3)];
    uint32_t state = UINT32_C(0x6d2b79f5);
    size_t iteration;

    for (iteration = 0; iteration < NPBC_ROBUSTNESS_ITERATIONS; iteration++) {
        size_t input_index =
            npbc_test_robustness_random(&state)
                % (sizeof(inputs) / sizeof(inputs[0]));
        size_t mutation_count =
            1 + npbc_test_robustness_random(&state) % 3U;
        size_t mutation;
        size_t used_size = inputs[input_index].size;

        NPBC_ASSERT(used_size <= sizeof(mutated));
        memcpy(mutated, inputs[input_index].data, used_size);
        for (mutation = 0; mutation < mutation_count; mutation++) {
            size_t offset = npbc_test_robustness_random(&state) % used_size;

            mutated[offset] = (uint8_t)(
                npbc_test_robustness_random(&state) & UINT8_C(0x7f)
            );
        }
        if (npbc_test_robustness_random(&state) % 5U == 0) {
            used_size = npbc_test_robustness_random(&state) % used_size;
        }
        NPBC_ASSERT(
            npbc_test_robustness_netpbm_attempt(mutated, used_size) == 0
        );
    }

    NPBC_ASSERT(
        npbc_test_robustness_netpbm_attempt(p5, sizeof(p5)) == 0
    );
    return 0;
}

static int npbc_test_robustness_repeated_cycles(void) {
    static const uint8_t input[] = "P1\n2 2\n0 1 1 0\n";
    size_t iteration;

    for (iteration = 0; iteration < 1000; iteration++) {
        NpbcImage image;
        FILE *stream = npbc_test_robustness_stream(
            input,
            sizeof(input) - 1
        );

        NPBC_ASSERT(stream != NULL);
        npbc_image_init(&image);
        NPBC_ASSERT(npbc_netpbm_read(stream, &image) == NPBC_STATUS_OK);
        npbc_image_destroy(&image);
        npbc_image_destroy(&image);
        NPBC_ASSERT(fclose(stream) == 0);
    }

    return 0;
}

static int npbc_test_robustness_large_inputs(void) {
    static const NpbcCompressionAlgorithm algorithms[] = {
        NPBC_COMPRESSION_RLE,
        NPBC_COMPRESSION_HUFFMAN
    };
    size_t input_size = (size_t)UINT64_C(1048576);
    uint8_t *input = malloc(input_size);
    size_t index;

    NPBC_ASSERT(input != NULL);
    for (index = 0; index < input_size; index++) {
        input[index] = index % 257U < 240U
            ? (uint8_t)(index % 4U)
            : (uint8_t)index;
    }

    for (index = 0; index < sizeof(algorithms) / sizeof(algorithms[0]); index++) {
        NpbcBuffer metadata;
        NpbcBuffer payload;
        NpbcBuffer decoded;

        npbc_buffer_init(&metadata);
        npbc_buffer_init(&payload);
        npbc_buffer_init(&decoded);
        NPBC_ASSERT(
            npbc_compress(
                algorithms[index],
                input,
                input_size,
                &metadata,
                &payload
            ) == NPBC_STATUS_OK
        );
        NPBC_ASSERT(
            npbc_decompress(
                algorithms[index],
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
    }

    free(input);
    return 0;
}

int npbc_test_robustness(void) {
    NPBC_ASSERT(npbc_test_robustness_rle_random() == 0);
    NPBC_ASSERT(npbc_test_robustness_huffman_random() == 0);
    NPBC_ASSERT(npbc_test_robustness_containers() == 0);
    NPBC_ASSERT(npbc_test_robustness_netpbm_corpus() == 0);
    NPBC_ASSERT(npbc_test_robustness_netpbm_truncations() == 0);
    NPBC_ASSERT(npbc_test_robustness_netpbm_random() == 0);
    NPBC_ASSERT(npbc_test_robustness_repeated_cycles() == 0);
    NPBC_ASSERT(npbc_test_robustness_large_inputs() == 0);
    return 0;
}
