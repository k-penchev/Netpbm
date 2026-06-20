#include "npbc/util.h"

#include <limits.h>

NpbcStatus npbc_size_add(size_t left, size_t right, size_t *result) {
    if (result == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (left > SIZE_MAX - right) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    *result = left + right;
    return NPBC_STATUS_OK;
}

NpbcStatus npbc_size_multiply(size_t left, size_t right, size_t *result) {
    if (result == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (left != 0 && right > SIZE_MAX / left) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    *result = left * right;
    return NPBC_STATUS_OK;
}

NpbcStatus npbc_raster_size(
    NpbcImageKind kind,
    uint32_t width,
    uint32_t height,
    uint16_t max_value,
    size_t *result
) {
    size_t row_size;
    size_t pixel_size;
    size_t raster_size;
    uint8_t channels;
    NpbcStatus status;

    if (result == NULL || width == 0 || height == 0) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    channels = npbc_image_channels(kind);
    if (channels == 0) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    if (kind == NPBC_IMAGE_BITMAP) {
        if (max_value != 1) {
            return NPBC_STATUS_INVALID_ARGUMENT;
        }
        row_size = ((size_t)width + 7) / 8;
    } else {
        if (max_value == 0) {
            return NPBC_STATUS_INVALID_ARGUMENT;
        }
        pixel_size = max_value < 256 ? 1 : 2;
        status = npbc_size_multiply(pixel_size, channels, &pixel_size);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        status = npbc_size_multiply((size_t)width, pixel_size, &row_size);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    }

    status = npbc_size_multiply(row_size, (size_t)height, &raster_size);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (raster_size > NPBC_MAX_RASTER_SIZE) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    *result = raster_size;
    return NPBC_STATUS_OK;
}

NpbcStatus npbc_read_exact(FILE *stream, void *data, size_t size) {
    size_t offset;

    if (stream == NULL || (data == NULL && size != 0)) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    offset = 0;
    while (offset < size) {
        size_t read_size = fread((uint8_t *)data + offset, 1, size - offset, stream);
        if (read_size == 0) {
            if (ferror(stream)) {
                return NPBC_STATUS_IO_FAILED;
            }
            return NPBC_STATUS_TRUNCATED_INPUT;
        }
        offset += read_size;
    }

    return NPBC_STATUS_OK;
}

NpbcStatus npbc_write_exact(FILE *stream, const void *data, size_t size) {
    size_t offset;

    if (stream == NULL || (data == NULL && size != 0)) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    offset = 0;
    while (offset < size) {
        size_t write_size = fwrite(
            (const uint8_t *)data + offset,
            1,
            size - offset,
            stream
        );
        if (write_size == 0) {
            return NPBC_STATUS_IO_FAILED;
        }
        offset += write_size;
    }

    return NPBC_STATUS_OK;
}

uint16_t npbc_read_u16_be(const uint8_t data[2]) {
    return (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

uint32_t npbc_read_u32_be(const uint8_t data[4]) {
    return ((uint32_t)data[0] << 24)
        | ((uint32_t)data[1] << 16)
        | ((uint32_t)data[2] << 8)
        | (uint32_t)data[3];
}

uint64_t npbc_read_u64_be(const uint8_t data[8]) {
    return ((uint64_t)data[0] << 56)
        | ((uint64_t)data[1] << 48)
        | ((uint64_t)data[2] << 40)
        | ((uint64_t)data[3] << 32)
        | ((uint64_t)data[4] << 24)
        | ((uint64_t)data[5] << 16)
        | ((uint64_t)data[6] << 8)
        | (uint64_t)data[7];
}

void npbc_write_u16_be(uint8_t data[2], uint16_t value) {
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

void npbc_write_u32_be(uint8_t data[4], uint32_t value) {
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

void npbc_write_u64_be(uint8_t data[8], uint64_t value) {
    data[0] = (uint8_t)(value >> 56);
    data[1] = (uint8_t)(value >> 48);
    data[2] = (uint8_t)(value >> 40);
    data[3] = (uint8_t)(value >> 32);
    data[4] = (uint8_t)(value >> 24);
    data[5] = (uint8_t)(value >> 16);
    data[6] = (uint8_t)(value >> 8);
    data[7] = (uint8_t)value;
}
