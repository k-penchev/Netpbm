#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npbc/compression.h"
#include "npbc/container.h"
#include "npbc/crc32.h"
#include "npbc/image.h"
#include "npbc/util.h"

static int npbc_test_container_make_image(
    NpbcImage *image,
    NpbcImageKind kind,
    uint32_t width,
    uint32_t height,
    uint16_t max_value,
    const uint8_t *raster,
    size_t raster_size
) {
    npbc_image_init(image);
    image->kind = kind;
    image->width = width;
    image->height = height;
    image->channels = npbc_image_channels(kind);
    image->max_value = max_value;
    image->raster_size = raster_size;
    image->raster = malloc(raster_size);
    NPBC_ASSERT(image->raster != NULL);
    memcpy(image->raster, raster, raster_size);
    return 0;
}

static int npbc_test_container_write_bytes(
    const NpbcImage *image,
    NpbcCompressionAlgorithm algorithm,
    uint8_t **data,
    size_t *size
) {
    FILE *stream;
    long file_size;

    stream = tmpfile();
    NPBC_ASSERT(stream != NULL);
    NPBC_ASSERT(
        npbc_container_write(stream, image, algorithm) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(fflush(stream) == 0);
    NPBC_ASSERT(fseek(stream, 0, SEEK_END) == 0);
    file_size = ftell(stream);
    NPBC_ASSERT(file_size >= 0);
    NPBC_ASSERT(fseek(stream, 0, SEEK_SET) == 0);

    *size = (size_t)file_size;
    *data = malloc(*size);
    NPBC_ASSERT(*data != NULL);
    NPBC_ASSERT(fread(*data, 1, *size, stream) == *size);
    NPBC_ASSERT(fclose(stream) == 0);
    return 0;
}

static FILE *npbc_test_container_stream(
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

static int npbc_test_container_read_bytes(
    const uint8_t *data,
    size_t size,
    NpbcStatus expected_status,
    NpbcImage *image,
    NpbcContainerInfo *info
) {
    FILE *stream = npbc_test_container_stream(data, size);
    NpbcStatus status;

    NPBC_ASSERT(stream != NULL);
    npbc_image_init(image);
    status = npbc_container_read(stream, image, info);
    NPBC_ASSERT(fclose(stream) == 0);
    NPBC_ASSERT(status == expected_status);
    return 0;
}

static int npbc_test_container_inspect_bytes(
    const uint8_t *data,
    size_t size,
    NpbcStatus expected_status,
    NpbcContainerInfo *info
) {
    FILE *stream = npbc_test_container_stream(data, size);
    NpbcStatus status;

    NPBC_ASSERT(stream != NULL);
    status = npbc_container_inspect(stream, info);
    NPBC_ASSERT(fclose(stream) == 0);
    NPBC_ASSERT(status == expected_status);
    return 0;
}

static int npbc_test_container_fixed_header(void) {
    const uint8_t raster[] = {7, 7, 7};
    const uint8_t expected[] = {
        'N', 'P', 'B', 'C',
        1, 1, 2, 0,
        0, 0, 0, 3,
        0, 0, 0, 1,
        0, 255,
        0, 0,
        0, 0, 0, 0, 0, 0, 0, 3,
        0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 2,
        0x2b, 0x2b, 0xcc, 0xf3,
        0x80, 7
    };
    NpbcImage image;
    uint8_t *data;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &image,
            NPBC_IMAGE_GRAYSCALE,
            3,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &image,
            NPBC_COMPRESSION_RLE,
            &data,
            &size
        ) == 0
    );
    NPBC_ASSERT(size == sizeof(expected));
    NPBC_ASSERT_BYTES(data, expected, sizeof(expected));

    free(data);
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_container_round_trip_algorithm(
    NpbcCompressionAlgorithm algorithm
) {
    const uint8_t raster[] = {
        0, 1, 2, 3, 4, 5,
        5, 5, 5, 5, 1, 0
    };
    NpbcImage source;
    NpbcImage decoded;
    NpbcContainerInfo info;
    uint8_t *data;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_RGB,
            2,
            2,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            algorithm,
            &data,
            &size
        ) == 0
    );
    NPBC_ASSERT(data[5] == (uint8_t)algorithm);
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size,
            NPBC_STATUS_OK,
            &decoded,
            &info
        ) == 0
    );
    NPBC_ASSERT(info.algorithm == algorithm);
    NPBC_ASSERT(decoded.kind == source.kind);
    NPBC_ASSERT(decoded.width == source.width);
    NPBC_ASSERT(decoded.height == source.height);
    NPBC_ASSERT(decoded.max_value == source.max_value);
    NPBC_ASSERT(decoded.raster_size == source.raster_size);
    NPBC_ASSERT_BYTES(decoded.raster, source.raster, source.raster_size);

    npbc_image_destroy(&decoded);
    npbc_image_destroy(&source);
    free(data);
    return 0;
}

static int npbc_test_container_image_variants(void) {
    const uint8_t bitmap_raster[] = {0xaa, 0x80};
    const uint8_t grayscale_raster[] = {0x00, 0x00, 0xff, 0xff};
    NpbcImage source;
    NpbcImage decoded;
    uint8_t *data;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_BITMAP,
            9,
            1,
            1,
            bitmap_raster,
            sizeof(bitmap_raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_RLE,
            &data,
            &size
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size,
            NPBC_STATUS_OK,
            &decoded,
            NULL
        ) == 0
    );
    NPBC_ASSERT_BYTES(
        decoded.raster,
        bitmap_raster,
        sizeof(bitmap_raster)
    );
    npbc_image_destroy(&decoded);
    npbc_image_destroy(&source);
    free(data);

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            2,
            1,
            65535,
            grayscale_raster,
            sizeof(grayscale_raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_HUFFMAN,
            &data,
            &size
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size,
            NPBC_STATUS_OK,
            &decoded,
            NULL
        ) == 0
    );
    NPBC_ASSERT(decoded.max_value == 65535);
    NPBC_ASSERT_BYTES(
        decoded.raster,
        grayscale_raster,
        sizeof(grayscale_raster)
    );
    npbc_image_destroy(&decoded);
    npbc_image_destroy(&source);
    free(data);
    return 0;
}

static int npbc_test_container_inspect(void) {
    const uint8_t raster[] = {1, 2, 3, 4};
    NpbcImage image;
    NpbcContainerInfo info;
    uint8_t *data;
    size_t size;
    FILE *stream;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &image,
            NPBC_IMAGE_GRAYSCALE,
            4,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &image,
            NPBC_COMPRESSION_HUFFMAN,
            &data,
            &size
        ) == 0
    );

    data[40] ^= 0xff;
    stream = npbc_test_container_stream(data, size);
    NPBC_ASSERT(stream != NULL);
    NPBC_ASSERT(
        npbc_container_inspect(stream, &info) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(fclose(stream) == 0);
    NPBC_ASSERT(info.algorithm == NPBC_COMPRESSION_HUFFMAN);
    NPBC_ASSERT(info.metadata_size == 264);
    NPBC_ASSERT(info.raster_size == sizeof(raster));

    free(data);
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_container_crc_failure(void) {
    const uint8_t raster[] = {1, 2, 3, 4};
    NpbcImage source;
    NpbcImage decoded;
    uint8_t *data;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            4,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_RLE,
            &data,
            &size
        ) == 0
    );
    data[40] ^= 1;
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size,
            NPBC_STATUS_INTEGRITY_FAILED,
            &decoded,
            NULL
        ) == 0
    );
    NPBC_ASSERT(decoded.raster == NULL);

    npbc_image_destroy(&decoded);
    npbc_image_destroy(&source);
    free(data);
    return 0;
}

static int npbc_test_container_huffman_crc_failure(void) {
    const uint8_t raster[] = {0, 1, 0, 1};
    NpbcImage source;
    NpbcImage decoded;
    uint8_t *data;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            4,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_HUFFMAN,
            &data,
            &size
        ) == 0
    );
    NPBC_ASSERT(size == 309);
    data[308] ^= 0x80;
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size,
            NPBC_STATUS_INTEGRITY_FAILED,
            &decoded,
            NULL
        ) == 0
    );
    npbc_image_destroy(&decoded);

    free(data);
    npbc_image_destroy(&source);
    return 0;
}

static int npbc_test_container_mutation(
    const uint8_t *original,
    size_t size,
    size_t offset,
    uint8_t value,
    NpbcStatus expected_status
) {
    uint8_t *data = malloc(size);
    NpbcImage image;

    NPBC_ASSERT(data != NULL);
    memcpy(data, original, size);
    data[offset] = value;
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size,
            expected_status,
            &image,
            NULL
        ) == 0
    );
    npbc_image_destroy(&image);
    free(data);
    return 0;
}

static int npbc_test_container_invalid_headers(void) {
    const uint8_t raster[] = {7, 7, 7};
    NpbcImage source;
    uint8_t *data;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            3,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_RLE,
            &data,
            &size
        ) == 0
    );

    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 0, 'X', NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 4, 2, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 5, 3, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 6, 4, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 7, 1, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 11, 0, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 19, 1, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 17, 0, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 27, 4, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_mutation(
            data, size, 31, 1, NPBC_STATUS_INVALID_FORMAT
        ) == 0
    );

    free(data);
    npbc_image_destroy(&source);
    return 0;
}

static int npbc_test_container_huffman_inspection(void) {
    const uint8_t raster[] = {0, 1, 2, 3};
    NpbcImage source;
    NpbcContainerInfo info;
    uint8_t *data;
    uint8_t *invalid;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            4,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_HUFFMAN,
            &data,
            &size
        ) == 0
    );

    invalid = malloc(size);
    NPBC_ASSERT(invalid != NULL);
    memcpy(invalid, data, size);
    invalid[52] = 64;
    NPBC_ASSERT(
        npbc_test_container_inspect_bytes(
            invalid,
            size,
            NPBC_STATUS_INVALID_FORMAT,
            &info
        ) == 0
    );

    memcpy(invalid, data, size);
    memset(invalid + 44, 0, 8);
    NPBC_ASSERT(
        npbc_test_container_inspect_bytes(
            invalid,
            size,
            NPBC_STATUS_INVALID_FORMAT,
            &info
        ) == 0
    );

    memcpy(invalid, data, size);
    invalid[31] = 7;
    NPBC_ASSERT(
        npbc_test_container_inspect_bytes(
            invalid,
            size,
            NPBC_STATUS_INVALID_FORMAT,
            &info
        ) == 0
    );

    free(invalid);
    free(data);
    npbc_image_destroy(&source);
    return 0;
}

static int npbc_test_container_size_headers(void) {
    uint8_t header[NPBC_CONTAINER_HEADER_SIZE];
    NpbcImage image;

    memset(header, 0, sizeof(header));
    memcpy(header, "NPBC", 4);
    header[4] = 1;
    header[5] = 1;
    header[6] = 2;
    npbc_write_u32_be(header + 8, UINT32_C(32768));
    npbc_write_u32_be(header + 12, UINT32_C(32769));
    npbc_write_u16_be(header + 16, 255);
    npbc_write_u64_be(
        header + 20,
        UINT64_C(32768) * UINT64_C(32769)
    );
    npbc_write_u64_be(header + 32, 1);
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            header,
            sizeof(header),
            NPBC_STATUS_SIZE_OVERFLOW,
            &image,
            NULL
        ) == 0
    );
    npbc_image_destroy(&image);

    npbc_write_u32_be(header + 8, 1);
    npbc_write_u32_be(header + 12, 1);
    npbc_write_u64_be(header + 20, 1);
    npbc_write_u64_be(header + 32, 0);
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            header,
            sizeof(header),
            NPBC_STATUS_INVALID_FORMAT,
            &image,
            NULL
        ) == 0
    );
    npbc_image_destroy(&image);
    return 0;
}

static int npbc_test_container_boundaries(void) {
    const uint8_t raster[] = {9, 9, 9};
    NpbcImage source;
    NpbcImage decoded;
    uint8_t *data;
    uint8_t *extended;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            3,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_HUFFMAN,
            &data,
            &size
        ) == 0
    );

    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            NPBC_CONTAINER_HEADER_SIZE - 1,
            NPBC_STATUS_TRUNCATED_INPUT,
            &decoded,
            NULL
        ) == 0
    );
    npbc_image_destroy(&decoded);
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            NPBC_CONTAINER_HEADER_SIZE + 100,
            NPBC_STATUS_TRUNCATED_INPUT,
            &decoded,
            NULL
        ) == 0
    );
    npbc_image_destroy(&decoded);
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size - 1,
            NPBC_STATUS_TRUNCATED_INPUT,
            &decoded,
            NULL
        ) == 0
    );
    npbc_image_destroy(&decoded);

    extended = malloc(size + 1);
    NPBC_ASSERT(extended != NULL);
    memcpy(extended, data, size);
    extended[size] = 0;
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            extended,
            size + 1,
            NPBC_STATUS_INVALID_FORMAT,
            &decoded,
            NULL
        ) == 0
    );
    npbc_image_destroy(&decoded);

    free(extended);
    free(data);
    npbc_image_destroy(&source);
    return 0;
}

static int npbc_test_container_codec_failure(void) {
    const uint8_t raster[] = {1, 1, 1};
    NpbcImage source;
    NpbcImage decoded;
    uint8_t *data;
    size_t size;

    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            3,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_RLE,
            &data,
            &size
        ) == 0
    );
    data[44] = 0xff;
    NPBC_ASSERT(
        npbc_test_container_read_bytes(
            data,
            size,
            NPBC_STATUS_INVALID_FORMAT,
            &decoded,
            NULL
        ) == 0
    );
    npbc_image_destroy(&decoded);

    free(data);
    npbc_image_destroy(&source);
    return 0;
}

static int npbc_test_container_expansion(void) {
    uint8_t raster[128];
    NpbcImage source;
    NpbcContainerInfo info;
    uint8_t *data;
    size_t size;
    FILE *stream;
    size_t index;

    for (index = 0; index < sizeof(raster); index++) {
        raster[index] = (uint8_t)(index & 1U);
    }
    NPBC_ASSERT(
        npbc_test_container_make_image(
            &source,
            NPBC_IMAGE_GRAYSCALE,
            128,
            1,
            255,
            raster,
            sizeof(raster)
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_write_bytes(
            &source,
            NPBC_COMPRESSION_RLE,
            &data,
            &size
        ) == 0
    );

    stream = npbc_test_container_stream(data, size);
    NPBC_ASSERT(stream != NULL);
    NPBC_ASSERT(
        npbc_container_inspect(stream, &info) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(fclose(stream) == 0);
    NPBC_ASSERT(info.algorithm == NPBC_COMPRESSION_RLE);
    NPBC_ASSERT(info.payload_size == sizeof(raster) + 1);

    free(data);
    npbc_image_destroy(&source);
    return 0;
}

static int npbc_test_compression_dispatch(void) {
    const uint8_t input[] = {0, 1, 0, 1};
    NpbcBuffer metadata;
    NpbcBuffer payload;
    NpbcBuffer decoded;

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    npbc_buffer_init(&decoded);
    NPBC_ASSERT(
        npbc_compress(
            NPBC_COMPRESSION_RLE,
            input,
            sizeof(input),
            &metadata,
            &payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(metadata.size == 0);
    NPBC_ASSERT(payload.size == sizeof(input) + 1);
    NPBC_ASSERT(
        npbc_decompress(
            NPBC_COMPRESSION_RLE,
            metadata.data,
            metadata.size,
            payload.data,
            payload.size,
            sizeof(input),
            &decoded
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT_BYTES(decoded.data, input, sizeof(input));
    npbc_buffer_destroy(&decoded);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    NPBC_ASSERT(
        npbc_compress(
            NPBC_COMPRESSION_HUFFMAN,
            input,
            sizeof(input),
            &metadata,
            &payload
        ) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(metadata.size == 264);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);
    return 0;
}

static int npbc_test_container_arguments(void) {
    NpbcImage image;
    NpbcContainerInfo info;
    FILE *stream = tmpfile();

    NPBC_ASSERT(stream != NULL);
    npbc_image_init(&image);
    NPBC_ASSERT(
        npbc_container_write(
            stream,
            &image,
            NPBC_COMPRESSION_RLE
        ) == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_container_write(
            stream,
            &image,
            (NpbcCompressionAlgorithm)99
        ) == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_container_read(NULL, &image, NULL)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    image.kind = NPBC_IMAGE_GRAYSCALE;
    NPBC_ASSERT(
        npbc_container_read(stream, &image, NULL)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    npbc_image_init(&image);
    NPBC_ASSERT(
        npbc_container_inspect(NULL, &info)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_container_inspect(stream, NULL)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(fclose(stream) == 0);

    NPBC_ASSERT(npbc_compression_algorithm_valid(NPBC_COMPRESSION_RLE));
    NPBC_ASSERT(npbc_compression_algorithm_valid(NPBC_COMPRESSION_HUFFMAN));
    NPBC_ASSERT(
        !npbc_compression_algorithm_valid((NpbcCompressionAlgorithm)0)
    );
    NPBC_ASSERT(
        strcmp(
            npbc_compression_algorithm_name(NPBC_COMPRESSION_RLE),
            "rle"
        ) == 0
    );
    NPBC_ASSERT(
        strcmp(
            npbc_compression_algorithm_name(NPBC_COMPRESSION_HUFFMAN),
            "huffman"
        ) == 0
    );
    NPBC_ASSERT(
        strcmp(
            npbc_compression_algorithm_name((NpbcCompressionAlgorithm)99),
            "unknown"
        ) == 0
    );
    return 0;
}

int npbc_test_container(void) {
    NPBC_ASSERT(npbc_test_container_fixed_header() == 0);
    NPBC_ASSERT(
        npbc_test_container_round_trip_algorithm(
            NPBC_COMPRESSION_RLE
        ) == 0
    );
    NPBC_ASSERT(
        npbc_test_container_round_trip_algorithm(
            NPBC_COMPRESSION_HUFFMAN
        ) == 0
    );
    NPBC_ASSERT(npbc_test_container_image_variants() == 0);
    NPBC_ASSERT(npbc_test_container_inspect() == 0);
    NPBC_ASSERT(npbc_test_container_crc_failure() == 0);
    NPBC_ASSERT(npbc_test_container_huffman_crc_failure() == 0);
    NPBC_ASSERT(npbc_test_container_invalid_headers() == 0);
    NPBC_ASSERT(npbc_test_container_huffman_inspection() == 0);
    NPBC_ASSERT(npbc_test_container_size_headers() == 0);
    NPBC_ASSERT(npbc_test_container_boundaries() == 0);
    NPBC_ASSERT(npbc_test_container_codec_failure() == 0);
    NPBC_ASSERT(npbc_test_container_expansion() == 0);
    NPBC_ASSERT(npbc_test_compression_dispatch() == 0);
    NPBC_ASSERT(npbc_test_container_arguments() == 0);
    return 0;
}
