#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "npbc/util.h"

static int npbc_test_checked_arithmetic(void) {
    size_t result;

    NPBC_ASSERT(npbc_size_add(0, 0, &result) == NPBC_STATUS_OK);
    NPBC_ASSERT(result == 0);
    NPBC_ASSERT(npbc_size_add(7, 9, &result) == NPBC_STATUS_OK);
    NPBC_ASSERT(result == 16);
    NPBC_ASSERT(
        npbc_size_add(SIZE_MAX, 1, &result) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(npbc_size_add(1, 2, NULL) == NPBC_STATUS_INVALID_ARGUMENT);

    NPBC_ASSERT(npbc_size_multiply(0, SIZE_MAX, &result) == NPBC_STATUS_OK);
    NPBC_ASSERT(result == 0);
    NPBC_ASSERT(npbc_size_multiply(7, 9, &result) == NPBC_STATUS_OK);
    NPBC_ASSERT(result == 63);
    NPBC_ASSERT(
        npbc_size_multiply(SIZE_MAX, 2, &result)
            == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_size_multiply(1, 2, NULL) == NPBC_STATUS_INVALID_ARGUMENT
    );

    return 0;
}

static int npbc_test_raster_sizes(void) {
    size_t result;

    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_BITMAP, 1, 1, 1, &result)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == 1);
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_BITMAP, 8, 2, 1, &result)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == 2);
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_BITMAP, 9, 2, 1, &result)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == 4);
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_GRAYSCALE, 4, 3, 255, &result)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == 12);
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_GRAYSCALE, 4, 3, 256, &result)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == 24);
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_RGB, 4, 3, 255, &result)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == 36);
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_RGB, 4, 3, 65535, &result)
            == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == 72);
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_GRAYSCALE,
            UINT32_C(32768),
            UINT32_C(32768),
            255,
            &result
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == NPBC_MAX_RASTER_SIZE);
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_GRAYSCALE,
            UINT32_C(32768),
            UINT32_C(32769),
            255,
            &result
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_BITMAP,
            UINT32_MAX,
            1,
            1,
            &result
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == (size_t)UINT64_C(536870912));
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_BITMAP,
            UINT32_MAX,
            2,
            1,
            &result
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == NPBC_MAX_RASTER_SIZE);
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_BITMAP,
            UINT32_MAX,
            3,
            1,
            &result
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_GRAYSCALE,
            UINT32_C(1073741824),
            1,
            255,
            &result
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(result == NPBC_MAX_RASTER_SIZE);
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_GRAYSCALE,
            UINT32_C(1073741825),
            1,
            255,
            &result
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_RGB, 0, 3, 255, &result)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_raster_size(NPBC_IMAGE_BITMAP, 1, 1, 2, &result)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_raster_size(
            NPBC_IMAGE_RGB,
            UINT32_MAX,
            UINT32_MAX,
            65535,
            &result
        ) == NPBC_STATUS_SIZE_OVERFLOW
    );

    return 0;
}

static int npbc_test_endian(void) {
    uint8_t data16[2];
    uint8_t data32[4];
    uint8_t data64[8];
    const uint8_t expected16[] = {0x12, 0x34};
    const uint8_t expected32[] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t expected64[] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
    };

    npbc_write_u16_be(data16, UINT16_C(0x1234));
    npbc_write_u32_be(data32, UINT32_C(0x12345678));
    npbc_write_u64_be(data64, UINT64_C(0x0123456789abcdef));

    NPBC_ASSERT_BYTES(data16, expected16, sizeof(data16));
    NPBC_ASSERT_BYTES(data32, expected32, sizeof(data32));
    NPBC_ASSERT_BYTES(data64, expected64, sizeof(data64));
    NPBC_ASSERT(npbc_read_u16_be(data16) == UINT16_C(0x1234));
    NPBC_ASSERT(npbc_read_u32_be(data32) == UINT32_C(0x12345678));
    NPBC_ASSERT(
        npbc_read_u64_be(data64) == UINT64_C(0x0123456789abcdef)
    );

    return 0;
}

static int npbc_test_exact_io(void) {
    FILE *stream;
    uint8_t input[] = {4, 3, 2, 1};
    uint8_t output[sizeof(input)];
    uint8_t extra;

    stream = tmpfile();
    NPBC_ASSERT(stream != NULL);
    NPBC_ASSERT(
        npbc_write_exact(stream, input, sizeof(input)) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(fflush(stream) == 0);
    NPBC_ASSERT(fseek(stream, 0, SEEK_SET) == 0);
    NPBC_ASSERT(
        npbc_read_exact(stream, output, sizeof(output)) == NPBC_STATUS_OK
    );
    NPBC_ASSERT_BYTES(input, output, sizeof(input));
    NPBC_ASSERT(
        npbc_read_exact(stream, &extra, 1) == NPBC_STATUS_TRUNCATED_INPUT
    );
    NPBC_ASSERT(fclose(stream) == 0);

    NPBC_ASSERT(npbc_read_exact(NULL, output, 1) == NPBC_STATUS_INVALID_ARGUMENT);
    NPBC_ASSERT(
        npbc_write_exact(NULL, input, 1) == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(npbc_read_exact(stdin, NULL, 0) == NPBC_STATUS_OK);
    NPBC_ASSERT(npbc_write_exact(stdout, NULL, 0) == NPBC_STATUS_OK);

    return 0;
}

int npbc_test_util(void) {
    NPBC_ASSERT(npbc_test_checked_arithmetic() == 0);
    NPBC_ASSERT(npbc_test_raster_sizes() == 0);
    NPBC_ASSERT(npbc_test_endian() == 0);
    NPBC_ASSERT(npbc_test_exact_io() == 0);
    NPBC_ASSERT(strcmp(npbc_status_name(NPBC_STATUS_OK), "ok") == 0);
    NPBC_ASSERT(
        strcmp(npbc_status_name((NpbcStatus)999), "unknown status") == 0
    );
    return 0;
}
