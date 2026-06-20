#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npbc/image.h"
#include "npbc/netpbm.h"

static FILE *npbc_test_stream(const uint8_t *data, size_t size) {
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

static int npbc_test_read_bytes(
    const uint8_t *data,
    size_t size,
    NpbcStatus expected_status,
    NpbcImage *image
) {
    FILE *stream = npbc_test_stream(data, size);
    NpbcStatus status;

    NPBC_ASSERT(stream != NULL);
    npbc_image_init(image);
    status = npbc_netpbm_read(stream, image);
    NPBC_ASSERT(fclose(stream) == 0);
    if (status != expected_status) {
        fprintf(
            stderr,
            "netpbm status mismatch: expected %d, received %d\n",
            expected_status,
            status
        );
    }
    NPBC_ASSERT(status == expected_status);
    return 0;
}

static int npbc_test_image_values(
    const NpbcImage *image,
    NpbcImageKind kind,
    uint32_t width,
    uint32_t height,
    uint16_t max_value,
    const uint8_t *raster,
    size_t raster_size
) {
    NPBC_ASSERT(image->kind == kind);
    NPBC_ASSERT(image->width == width);
    NPBC_ASSERT(image->height == height);
    NPBC_ASSERT(image->channels == npbc_image_channels(kind));
    NPBC_ASSERT(image->max_value == max_value);
    NPBC_ASSERT(image->raster_size == raster_size);
    NPBC_ASSERT_BYTES(image->raster, raster, raster_size);
    return 0;
}

static int npbc_test_basic_ascii(void) {
    const uint8_t p1[] = "P1\n# dimensions\n3 2\n0 1 0\n1 # row\n0 1\n";
    const uint8_t p2[] = "P2\v2\f2\r15\n0\t15 # values\n7 8\n";
    const uint8_t p3[] = "P3\n2 1\n255\n255 0 1 2 3 4\n";
    const uint8_t p1_raster[] = {0x40, 0xa0};
    const uint8_t p2_raster[] = {0, 15, 7, 8};
    const uint8_t p3_raster[] = {255, 0, 1, 2, 3, 4};
    NpbcImage image;

    NPBC_ASSERT(
        npbc_test_read_bytes(
            p1,
            sizeof(p1) - 1,
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_BITMAP,
            3,
            2,
            1,
            p1_raster,
            sizeof(p1_raster)
        ) == 0
    );
    npbc_image_destroy(&image);

    NPBC_ASSERT(
        npbc_test_read_bytes(
            p2,
            sizeof(p2) - 1,
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_GRAYSCALE,
            2,
            2,
            15,
            p2_raster,
            sizeof(p2_raster)
        ) == 0
    );
    npbc_image_destroy(&image);

    NPBC_ASSERT(
        npbc_test_read_bytes(
            p3,
            sizeof(p3) - 1,
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_RGB,
            2,
            1,
            255,
            p3_raster,
            sizeof(p3_raster)
        ) == 0
    );
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_basic_binary(void) {
    const uint8_t p4[] = {'P', '4', '\n', '9', ' ', '1', '\n', 0xff, 0xff};
    const uint8_t p5[] = {'P', '5', '\n', '2', ' ', '1', '\n', '2', '5', '5', '\n', ' ', '#'};
    const uint8_t p6[] = {
        'P', '6', '\n', '1', ' ', '1', '\n', '6', '5', '5', '3', '5', '\n',
        0x00, 0x00, 0x80, 0x00, 0xff, 0xff
    };
    const uint8_t p4_raster[] = {0xff, 0x80};
    const uint8_t p5_raster[] = {' ', '#'};
    const uint8_t p6_raster[] = {0x00, 0x00, 0x80, 0x00, 0xff, 0xff};
    NpbcImage image;

    NPBC_ASSERT(
        npbc_test_read_bytes(
            p4,
            sizeof(p4),
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_BITMAP,
            9,
            1,
            1,
            p4_raster,
            sizeof(p4_raster)
        ) == 0
    );
    npbc_image_destroy(&image);

    NPBC_ASSERT(
        npbc_test_read_bytes(
            p5,
            sizeof(p5),
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_GRAYSCALE,
            2,
            1,
            255,
            p5_raster,
            sizeof(p5_raster)
        ) == 0
    );
    npbc_image_destroy(&image);

    NPBC_ASSERT(
        npbc_test_read_bytes(
            p6,
            sizeof(p6),
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_RGB,
            1,
            1,
            65535,
            p6_raster,
            sizeof(p6_raster)
        ) == 0
    );
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_token_comments_and_separator(void) {
    const uint8_t ascii[] = "P2\n2# width\n1# height\n255# maximum\n10# first\n20# final";
    const uint8_t binary[] = {
        'P', '5', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\r', '\n'
    };
    const uint8_t ascii_raster[] = {10, 20};
    const uint8_t binary_raster[] = {'\n'};
    NpbcImage image;

    NPBC_ASSERT(
        npbc_test_read_bytes(
            ascii,
            sizeof(ascii) - 1,
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_GRAYSCALE,
            2,
            1,
            255,
            ascii_raster,
            sizeof(ascii_raster)
        ) == 0
    );
    npbc_image_destroy(&image);

    NPBC_ASSERT(
        npbc_test_read_bytes(
            binary,
            sizeof(binary),
            NPBC_STATUS_OK,
            &image
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_image_values(
            &image,
            NPBC_IMAGE_GRAYSCALE,
            1,
            1,
            255,
            binary_raster,
            sizeof(binary_raster)
        ) == 0
    );
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_bitmap_widths(void) {
    const uint32_t widths[] = {1, 7, 8, 9, 17};
    size_t index;

    for (index = 0; index < sizeof(widths) / sizeof(widths[0]); index++) {
        uint32_t width = widths[index];
        char header[32];
        uint8_t input[40];
        size_t row_size = ((size_t)width + 7) / 8;
        int header_size = snprintf(header, sizeof(header), "P4\n%u 1\n", width);
        NpbcImage image;
        size_t byte_index;

        NPBC_ASSERT(header_size > 0);
        memcpy(input, header, (size_t)header_size);
        for (byte_index = 0; byte_index < row_size; byte_index++) {
            input[(size_t)header_size + byte_index] = 0xff;
        }

        NPBC_ASSERT(
            npbc_test_read_bytes(
                input,
                (size_t)header_size + row_size,
                NPBC_STATUS_OK,
                &image
            ) == 0
        );
        NPBC_ASSERT(image.raster_size == row_size);
        if (width % 8U != 0) {
            uint8_t mask = (uint8_t)(UINT8_C(0xff) << (8U - width % 8U));
            NPBC_ASSERT(image.raster[row_size - 1] == mask);
        }
        npbc_image_destroy(&image);
    }

    return 0;
}

static int npbc_test_invalid_inputs(void) {
    static const struct {
        const char *input;
        NpbcStatus status;
    } cases[] = {
        {"", NPBC_STATUS_TRUNCATED_INPUT},
        {"P", NPBC_STATUS_TRUNCATED_INPUT},
        {"P7\n1 1\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1#\n1 1\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1\n0 1\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1\n1 0\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1\n-1 1\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1\n+1 1\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1\n1x 1\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1\n4294967296 1\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {
            "P4\n4294967295 4294967295\n",
            NPBC_STATUS_SIZE_OVERFLOW
        },
        {"P1\n1 1\n2\n", NPBC_STATUS_INVALID_FORMAT},
        {"P1\n1 1\n", NPBC_STATUS_TRUNCATED_INPUT},
        {"P1\n1 1\n0 1\n", NPBC_STATUS_INVALID_FORMAT},
        {"P2\n1 1\n0\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P2\n1 1\n65536\n0\n", NPBC_STATUS_INVALID_FORMAT},
        {"P2\n1 1\n15\n16\n", NPBC_STATUS_INVALID_FORMAT},
        {"P3\n1 1\n255\n0 0\n", NPBC_STATUS_TRUNCATED_INPUT},
        {"P3\n1 1\n255\n0 0 0 x\n", NPBC_STATUS_INVALID_FORMAT},
        {"P4\n1 1", NPBC_STATUS_TRUNCATED_INPUT},
        {"P5\n1 1\n255#", NPBC_STATUS_INVALID_FORMAT},
        {"P5\n1 1\n255\n", NPBC_STATUS_TRUNCATED_INPUT},
        {"P5\n1 1\n15\n\x10", NPBC_STATUS_INVALID_FORMAT}
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        NpbcImage image;
        int result;

        result = npbc_test_read_bytes(
            (const uint8_t *)cases[index].input,
            strlen(cases[index].input),
            cases[index].status,
            &image
        );
        if (result != 0) {
            fprintf(stderr, "invalid input index: %zu\n", index);
        }
        NPBC_ASSERT(result == 0);
        npbc_image_destroy(&image);
    }

    return 0;
}

static int npbc_test_binary_truncation_and_trailing(void) {
    const uint8_t truncated[] = {
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 1, 2
    };
    const uint8_t trailing[] = {
        'P', '5', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 1, 2
    };
    NpbcImage image;

    NPBC_ASSERT(
        npbc_test_read_bytes(
            truncated,
            sizeof(truncated),
            NPBC_STATUS_TRUNCATED_INPUT,
            &image
        ) == 0
    );
    npbc_image_destroy(&image);

    NPBC_ASSERT(
        npbc_test_read_bytes(
            trailing,
            sizeof(trailing),
            NPBC_STATUS_INVALID_FORMAT,
            &image
        ) == 0
    );
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_write_and_reparse(void) {
    const uint8_t source[] = "P3\n2 1\n256\n0 1 2 256 255 3\n";
    const uint8_t expected[] = {
        'P', '6', '\n', '2', ' ', '1', '\n', '2', '5', '6', '\n',
        0, 0, 0, 1, 0, 2, 1, 0, 0, 255, 0, 3
    };
    NpbcImage first;
    NpbcImage second;
    FILE *output;
    uint8_t actual[sizeof(expected)];
    int extra;

    NPBC_ASSERT(
        npbc_test_read_bytes(
            source,
            sizeof(source) - 1,
            NPBC_STATUS_OK,
            &first
        ) == 0
    );

    output = tmpfile();
    NPBC_ASSERT(output != NULL);
    NPBC_ASSERT(npbc_netpbm_write(output, &first) == NPBC_STATUS_OK);
    NPBC_ASSERT(fflush(output) == 0);
    NPBC_ASSERT(fseek(output, 0, SEEK_SET) == 0);
    NPBC_ASSERT(fread(actual, 1, sizeof(actual), output) == sizeof(actual));
    extra = fgetc(output);
    NPBC_ASSERT(extra == EOF);
    NPBC_ASSERT_BYTES(actual, expected, sizeof(expected));
    NPBC_ASSERT(fseek(output, 0, SEEK_SET) == 0);

    npbc_image_init(&second);
    NPBC_ASSERT(npbc_netpbm_read(output, &second) == NPBC_STATUS_OK);
    NPBC_ASSERT(fclose(output) == 0);
    NPBC_ASSERT(
        npbc_test_image_values(
            &second,
            first.kind,
            first.width,
            first.height,
            first.max_value,
            first.raster,
            first.raster_size
        ) == 0
    );

    npbc_image_destroy(&second);
    npbc_image_destroy(&first);
    return 0;
}

static int npbc_test_bitmap_writer_padding(void) {
    NpbcImage image;
    FILE *output;
    uint8_t actual[9];
    const uint8_t expected[] = {'P', '4', '\n', '1', ' ', '1', '\n', 0x80};

    npbc_image_init(&image);
    image.kind = NPBC_IMAGE_BITMAP;
    image.width = 1;
    image.height = 1;
    image.channels = 1;
    image.max_value = 1;
    image.raster_size = 1;
    image.raster = malloc(1);
    NPBC_ASSERT(image.raster != NULL);
    image.raster[0] = 0xff;

    output = tmpfile();
    NPBC_ASSERT(output != NULL);
    NPBC_ASSERT(npbc_netpbm_write(output, &image) == NPBC_STATUS_OK);
    NPBC_ASSERT(fflush(output) == 0);
    NPBC_ASSERT(fseek(output, 0, SEEK_SET) == 0);
    NPBC_ASSERT(fread(actual, 1, sizeof(expected), output) == sizeof(expected));
    NPBC_ASSERT_BYTES(actual, expected, sizeof(expected));
    NPBC_ASSERT(fgetc(output) == EOF);
    NPBC_ASSERT(fclose(output) == 0);

    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_grayscale_writer(void) {
    NpbcImage image;
    FILE *output;
    uint8_t actual[13];
    const uint8_t expected[] = {
        'P', '5', '\n', '2', ' ', '1', '\n', '2', '5', '5', '\n', 0, 255
    };

    npbc_image_init(&image);
    image.kind = NPBC_IMAGE_GRAYSCALE;
    image.width = 2;
    image.height = 1;
    image.channels = 1;
    image.max_value = 255;
    image.raster_size = 2;
    image.raster = malloc(2);
    NPBC_ASSERT(image.raster != NULL);
    image.raster[0] = 0;
    image.raster[1] = 255;

    output = tmpfile();
    NPBC_ASSERT(output != NULL);
    NPBC_ASSERT(npbc_netpbm_write(output, &image) == NPBC_STATUS_OK);
    NPBC_ASSERT(fflush(output) == 0);
    NPBC_ASSERT(fseek(output, 0, SEEK_SET) == 0);
    NPBC_ASSERT(fread(actual, 1, sizeof(actual), output) == sizeof(actual));
    NPBC_ASSERT_BYTES(actual, expected, sizeof(expected));
    NPBC_ASSERT(fgetc(output) == EOF);
    NPBC_ASSERT(fclose(output) == 0);

    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_equivalent_encodings(void) {
    const uint8_t ascii[] = "P2\n3 1\n255\n10 35 255\n";
    const uint8_t binary[] = {
        'P', '5', '\n', '3', ' ', '1', '\n', '2', '5', '5', '\n',
        10, 35, 255
    };
    NpbcImage first;
    NpbcImage second;

    NPBC_ASSERT(
        npbc_test_read_bytes(
            ascii,
            sizeof(ascii) - 1,
            NPBC_STATUS_OK,
            &first
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_read_bytes(
            binary,
            sizeof(binary),
            NPBC_STATUS_OK,
            &second
        ) == 0
    );
    NPBC_ASSERT(first.kind == second.kind);
    NPBC_ASSERT(first.width == second.width);
    NPBC_ASSERT(first.height == second.height);
    NPBC_ASSERT(first.max_value == second.max_value);
    NPBC_ASSERT(first.raster_size == second.raster_size);
    NPBC_ASSERT_BYTES(first.raster, second.raster, first.raster_size);

    npbc_image_destroy(&second);
    npbc_image_destroy(&first);
    return 0;
}

static int npbc_test_fixtures(void) {
    static const char *paths[] = {
        "tests/fixtures/netpbm/p1.pbm",
        "tests/fixtures/netpbm/p2.pgm",
        "tests/fixtures/netpbm/p3.ppm",
        "tests/fixtures/netpbm/p4.pbm",
        "tests/fixtures/netpbm/p5.pgm",
        "tests/fixtures/netpbm/p6.ppm"
    };
    static const NpbcImageKind kinds[] = {
        NPBC_IMAGE_BITMAP,
        NPBC_IMAGE_GRAYSCALE,
        NPBC_IMAGE_RGB,
        NPBC_IMAGE_BITMAP,
        NPBC_IMAGE_GRAYSCALE,
        NPBC_IMAGE_RGB
    };
    size_t index;

    for (index = 0; index < sizeof(paths) / sizeof(paths[0]); index++) {
        FILE *stream = fopen(paths[index], "rb");
        NpbcImage image;

        NPBC_ASSERT(stream != NULL);
        npbc_image_init(&image);
        NPBC_ASSERT(npbc_netpbm_read(stream, &image) == NPBC_STATUS_OK);
        NPBC_ASSERT(fclose(stream) == 0);
        NPBC_ASSERT(image.kind == kinds[index]);
        npbc_image_destroy(&image);
    }

    return 0;
}

static int npbc_test_writer_rejection(void) {
    NpbcImage image;
    FILE *output;

    npbc_image_init(&image);
    output = tmpfile();
    NPBC_ASSERT(output != NULL);
    NPBC_ASSERT(
        npbc_netpbm_write(output, &image) == NPBC_STATUS_INVALID_ARGUMENT
    );

    image.kind = NPBC_IMAGE_GRAYSCALE;
    image.width = 1;
    image.height = 1;
    image.channels = 1;
    image.max_value = 15;
    image.raster_size = 1;
    image.raster = malloc(1);
    NPBC_ASSERT(image.raster != NULL);
    image.raster[0] = 16;
    NPBC_ASSERT(
        npbc_netpbm_write(output, &image) == NPBC_STATUS_INVALID_ARGUMENT
    );

    image.raster[0] = 15;
    image.channels = 3;
    NPBC_ASSERT(
        npbc_netpbm_write(output, &image) == NPBC_STATUS_INVALID_ARGUMENT
    );

    NPBC_ASSERT(fclose(output) == 0);
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_reader_ownership(void) {
    const uint8_t input[] = "P1\n1 1\n0\n";
    NpbcImage image;
    FILE *stream;

    npbc_image_init(&image);
    image.kind = NPBC_IMAGE_BITMAP;
    stream = npbc_test_stream(input, sizeof(input) - 1);
    NPBC_ASSERT(stream != NULL);
    NPBC_ASSERT(
        npbc_netpbm_read(stream, &image) == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(fclose(stream) == 0);
    NPBC_ASSERT(image.kind == NPBC_IMAGE_BITMAP);

    npbc_image_init(&image);
    NPBC_ASSERT(npbc_netpbm_read(NULL, &image) == NPBC_STATUS_INVALID_ARGUMENT);
    NPBC_ASSERT(npbc_netpbm_read(stdin, NULL) == NPBC_STATUS_INVALID_ARGUMENT);
    return 0;
}

int npbc_test_netpbm(void) {
    NPBC_ASSERT(npbc_test_basic_ascii() == 0);
    NPBC_ASSERT(npbc_test_basic_binary() == 0);
    NPBC_ASSERT(npbc_test_token_comments_and_separator() == 0);
    NPBC_ASSERT(npbc_test_bitmap_widths() == 0);
    NPBC_ASSERT(npbc_test_invalid_inputs() == 0);
    NPBC_ASSERT(npbc_test_binary_truncation_and_trailing() == 0);
    NPBC_ASSERT(npbc_test_write_and_reparse() == 0);
    NPBC_ASSERT(npbc_test_bitmap_writer_padding() == 0);
    NPBC_ASSERT(npbc_test_grayscale_writer() == 0);
    NPBC_ASSERT(npbc_test_equivalent_encodings() == 0);
    NPBC_ASSERT(npbc_test_fixtures() == 0);
    NPBC_ASSERT(npbc_test_writer_rejection() == 0);
    NPBC_ASSERT(npbc_test_reader_ownership() == 0);
    return 0;
}
