#include "npbc/container.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "npbc/buffer.h"
#include "npbc/crc32.h"
#include "npbc/huffman.h"
#include "npbc/util.h"

typedef struct {
    NpbcContainerInfo info;
    size_t raster_size;
    size_t metadata_size;
    size_t payload_size;
} NpbcContainerHeader;

static int npbc_container_image_is_empty(const NpbcImage *image) {
    return image != NULL
        && image->kind == 0
        && image->width == 0
        && image->height == 0
        && image->channels == 0
        && image->max_value == 0
        && image->raster == NULL
        && image->raster_size == 0;
}

static NpbcStatus npbc_container_validate_image(const NpbcImage *image) {
    size_t expected_size;
    size_t sample_size;
    size_t offset;
    uint8_t channels;
    NpbcStatus status;

    if (image == NULL || image->raster == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    channels = npbc_image_channels(image->kind);
    if (channels == 0 || channels != image->channels) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    status = npbc_raster_size(
        image->kind,
        image->width,
        image->height,
        image->max_value,
        &expected_size
    );
    if (status != NPBC_STATUS_OK || expected_size != image->raster_size) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    if (image->kind == NPBC_IMAGE_BITMAP) {
        unsigned int remainder = image->width % 8U;

        if (remainder != 0) {
            size_t row_size = ((size_t)image->width + 7) / 8;
            uint8_t padding_mask = (uint8_t)(
                (UINT8_C(1) << (8U - remainder)) - 1
            );
            uint32_t row;

            for (row = 0; row < image->height; row++) {
                if (
                    (
                        image->raster[(size_t)(row + 1) * row_size - 1]
                        & padding_mask
                    ) != 0
                ) {
                    return NPBC_STATUS_INVALID_ARGUMENT;
                }
            }
        }
        return NPBC_STATUS_OK;
    }

    sample_size = image->max_value < 256 ? 1 : 2;
    for (offset = 0; offset < image->raster_size; offset += sample_size) {
        uint32_t sample = sample_size == 1
            ? image->raster[offset]
            : npbc_read_u16_be(image->raster + offset);

        if (sample > image->max_value) {
            return NPBC_STATUS_INVALID_ARGUMENT;
        }
    }

    return NPBC_STATUS_OK;
}

static void npbc_container_serialize_header(
    uint8_t header[NPBC_CONTAINER_HEADER_SIZE],
    const NpbcContainerInfo *info
) {
    memset(header, 0, NPBC_CONTAINER_HEADER_SIZE);
    memcpy(header, "NPBC", 4);
    header[4] = info->compatibility;
    header[5] = (uint8_t)info->algorithm;
    header[6] = (uint8_t)info->image_kind;
    header[7] = 0;
    npbc_write_u32_be(header + 8, info->width);
    npbc_write_u32_be(header + 12, info->height);
    npbc_write_u16_be(header + 16, info->max_value);
    npbc_write_u16_be(header + 18, 0);
    npbc_write_u64_be(header + 20, info->raster_size);
    npbc_write_u32_be(header + 28, info->metadata_size);
    npbc_write_u64_be(header + 32, info->payload_size);
    npbc_write_u32_be(header + 40, info->crc32);
}

static NpbcStatus npbc_container_algorithm_size_limit(
    NpbcCompressionAlgorithm algorithm,
    uint64_t raster_size,
    uint64_t *payload_limit
) {
    if (algorithm == NPBC_COMPRESSION_RLE) {
        *payload_limit = raster_size + (raster_size + 127) / 128;
        return NPBC_STATUS_OK;
    }
    if (algorithm == NPBC_COMPRESSION_HUFFMAN) {
        *payload_limit = (raster_size * 63 + 7) / 8;
        return NPBC_STATUS_OK;
    }
    return NPBC_STATUS_INVALID_FORMAT;
}

static NpbcStatus npbc_container_validate_info(
    const NpbcContainerInfo *info,
    NpbcContainerHeader *parsed
) {
    uint64_t payload_limit;
    size_t raster_size;
    NpbcStatus status;

    if (info->compatibility != NPBC_CONTAINER_COMPATIBILITY) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (!npbc_compression_algorithm_valid(info->algorithm)) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (
        info->image_kind != NPBC_IMAGE_BITMAP
        && info->image_kind != NPBC_IMAGE_GRAYSCALE
        && info->image_kind != NPBC_IMAGE_RGB
    ) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (info->raster_size > SIZE_MAX || info->payload_size > SIZE_MAX) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    status = npbc_raster_size(
        info->image_kind,
        info->width,
        info->height,
        info->max_value,
        &raster_size
    );
    if (status == NPBC_STATUS_INVALID_ARGUMENT) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if ((uint64_t)raster_size != info->raster_size) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    if (info->algorithm == NPBC_COMPRESSION_RLE) {
        if (info->metadata_size != 0) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
    } else if (info->metadata_size != NPBC_HUFFMAN_METADATA_SIZE) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    status = npbc_container_algorithm_size_limit(
        info->algorithm,
        info->raster_size,
        &payload_limit
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (info->payload_size == 0 || info->payload_size > payload_limit) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    parsed->info = *info;
    parsed->raster_size = raster_size;
    parsed->metadata_size = info->metadata_size;
    parsed->payload_size = (size_t)info->payload_size;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_container_parse_header(
    const uint8_t header[NPBC_CONTAINER_HEADER_SIZE],
    NpbcContainerHeader *parsed
) {
    NpbcContainerInfo info;

    if (memcmp(header, "NPBC", 4) != 0) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (header[7] != 0 || npbc_read_u16_be(header + 18) != 0) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    info.compatibility = header[4];
    info.algorithm = (NpbcCompressionAlgorithm)header[5];
    info.image_kind = (NpbcImageKind)header[6];
    info.width = npbc_read_u32_be(header + 8);
    info.height = npbc_read_u32_be(header + 12);
    info.max_value = npbc_read_u16_be(header + 16);
    info.raster_size = npbc_read_u64_be(header + 20);
    info.metadata_size = npbc_read_u32_be(header + 28);
    info.payload_size = npbc_read_u64_be(header + 32);
    info.crc32 = npbc_read_u32_be(header + 40);
    return npbc_container_validate_info(&info, parsed);
}

static NpbcStatus npbc_container_read_header(
    FILE *stream,
    NpbcContainerHeader *parsed
) {
    uint8_t header[NPBC_CONTAINER_HEADER_SIZE];
    NpbcStatus status;

    status = npbc_read_exact(stream, header, sizeof(header));
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    return npbc_container_parse_header(header, parsed);
}

static NpbcStatus npbc_container_expect_end(FILE *stream) {
    int character = fgetc(stream);

    if (character != EOF) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (ferror(stream)) {
        return NPBC_STATUS_IO_FAILED;
    }
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_container_skip_exact(FILE *stream, uint64_t size) {
    uint8_t data[4096];

    while (size != 0) {
        size_t chunk = size > sizeof(data) ? sizeof(data) : (size_t)size;
        NpbcStatus status = npbc_read_exact(stream, data, chunk);

        if (status != NPBC_STATUS_OK) {
            return status;
        }
        size -= chunk;
    }

    return NPBC_STATUS_OK;
}

NpbcStatus npbc_container_write(
    FILE *stream,
    const NpbcImage *image,
    NpbcCompressionAlgorithm algorithm
) {
    NpbcBuffer metadata;
    NpbcBuffer payload;
    NpbcContainerInfo info;
    uint8_t header[NPBC_CONTAINER_HEADER_SIZE];
    NpbcStatus status;

    if (
        stream == NULL
        || !npbc_compression_algorithm_valid(algorithm)
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    status = npbc_container_validate_image(image);
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    status = npbc_compress(
        algorithm,
        image->raster,
        image->raster_size,
        &metadata,
        &payload
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (metadata.size > UINT32_MAX) {
        npbc_buffer_destroy(&payload);
        npbc_buffer_destroy(&metadata);
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    info.compatibility = NPBC_CONTAINER_COMPATIBILITY;
    info.algorithm = algorithm;
    info.image_kind = image->kind;
    info.width = image->width;
    info.height = image->height;
    info.max_value = image->max_value;
    info.raster_size = image->raster_size;
    info.metadata_size = (uint32_t)metadata.size;
    info.payload_size = payload.size;
    info.crc32 = npbc_crc32(image->raster, image->raster_size);
    npbc_container_serialize_header(header, &info);

    status = npbc_write_exact(stream, header, sizeof(header));
    if (status == NPBC_STATUS_OK) {
        status = npbc_write_exact(stream, metadata.data, metadata.size);
    }
    if (status == NPBC_STATUS_OK) {
        status = npbc_write_exact(stream, payload.data, payload.size);
    }

    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);
    return status;
}

NpbcStatus npbc_container_inspect(
    FILE *stream,
    NpbcContainerInfo *info
) {
    NpbcContainerHeader parsed;
    NpbcBuffer metadata;
    NpbcStatus status;

    if (stream == NULL || info == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    status = npbc_container_read_header(stream, &parsed);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    npbc_buffer_init(&metadata);
    if (parsed.metadata_size != 0) {
        uint64_t bit_count;
        size_t expected_payload_size;

        status = npbc_buffer_reserve(&metadata, parsed.metadata_size);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        metadata.size = parsed.metadata_size;
        status = npbc_read_exact(stream, metadata.data, metadata.size);
        if (status == NPBC_STATUS_OK) {
            status = npbc_huffman_validate_metadata(
                metadata.data,
                metadata.size,
                parsed.raster_size,
                &bit_count,
                &expected_payload_size
            );
        }
        if (
            status == NPBC_STATUS_OK
            && expected_payload_size != parsed.payload_size
        ) {
            status = NPBC_STATUS_INVALID_FORMAT;
        }
        npbc_buffer_destroy(&metadata);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    }

    status = npbc_container_skip_exact(stream, parsed.payload_size);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_container_expect_end(stream);
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    *info = parsed.info;
    return NPBC_STATUS_OK;
}

NpbcStatus npbc_container_read(
    FILE *stream,
    NpbcImage *image,
    NpbcContainerInfo *info
) {
    NpbcContainerHeader parsed;
    NpbcBuffer metadata;
    NpbcBuffer payload;
    NpbcBuffer raster;
    NpbcImage decoded_image;
    NpbcStatus status;

    if (
        stream == NULL
        || !npbc_container_image_is_empty(image)
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    status = npbc_container_read_header(stream, &parsed);
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    npbc_buffer_init(&metadata);
    npbc_buffer_init(&payload);
    npbc_buffer_init(&raster);
    npbc_image_init(&decoded_image);

    if (parsed.metadata_size != 0) {
        status = npbc_buffer_reserve(&metadata, parsed.metadata_size);
        if (status != NPBC_STATUS_OK) {
            goto cleanup;
        }
        metadata.size = parsed.metadata_size;
        status = npbc_read_exact(stream, metadata.data, metadata.size);
        if (status != NPBC_STATUS_OK) {
            goto cleanup;
        }
    }

    status = npbc_buffer_reserve(&payload, parsed.payload_size);
    if (status != NPBC_STATUS_OK) {
        goto cleanup;
    }
    payload.size = parsed.payload_size;
    status = npbc_read_exact(stream, payload.data, payload.size);
    if (status != NPBC_STATUS_OK) {
        goto cleanup;
    }
    status = npbc_container_expect_end(stream);
    if (status != NPBC_STATUS_OK) {
        goto cleanup;
    }

    status = npbc_decompress(
        parsed.info.algorithm,
        metadata.data,
        metadata.size,
        payload.data,
        payload.size,
        parsed.raster_size,
        &raster
    );
    if (status != NPBC_STATUS_OK) {
        goto cleanup;
    }
    if (npbc_crc32(raster.data, raster.size) != parsed.info.crc32) {
        status = NPBC_STATUS_INTEGRITY_FAILED;
        goto cleanup;
    }

    decoded_image.kind = parsed.info.image_kind;
    decoded_image.width = parsed.info.width;
    decoded_image.height = parsed.info.height;
    decoded_image.channels = npbc_image_channels(decoded_image.kind);
    decoded_image.max_value = parsed.info.max_value;
    decoded_image.raster = raster.data;
    decoded_image.raster_size = raster.size;
    npbc_buffer_init(&raster);
    *image = decoded_image;
    npbc_image_init(&decoded_image);
    if (info != NULL) {
        *info = parsed.info;
    }
    status = NPBC_STATUS_OK;

cleanup:
    npbc_image_destroy(&decoded_image);
    npbc_buffer_destroy(&raster);
    npbc_buffer_destroy(&payload);
    npbc_buffer_destroy(&metadata);
    return status;
}
