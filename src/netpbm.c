#include "npbc/netpbm.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "npbc/util.h"

typedef struct {
    FILE *stream;
} NpbcNetpbmReader;

static int npbc_netpbm_is_whitespace(int character) {
    return character == ' '
        || character == '\t'
        || character == '\r'
        || character == '\n'
        || character == '\v'
        || character == '\f';
}

static NpbcStatus npbc_netpbm_read_character(
    NpbcNetpbmReader *reader,
    int *character
) {
    int value;

    value = fgetc(reader->stream);
    if (value == EOF && ferror(reader->stream)) {
        return NPBC_STATUS_IO_FAILED;
    }

    *character = value;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_skip_comment(NpbcNetpbmReader *reader) {
    int character;
    NpbcStatus status;

    do {
        status = npbc_netpbm_read_character(reader, &character);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    } while (character != EOF && character != '\n' && character != '\r');

    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_next_token_start(
    NpbcNetpbmReader *reader,
    int *character
) {
    NpbcStatus status;

    for (;;) {
        status = npbc_netpbm_read_character(reader, character);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        if (*character == EOF) {
            return NPBC_STATUS_TRUNCATED_INPUT;
        }
        if (npbc_netpbm_is_whitespace(*character)) {
            continue;
        }
        if (*character == '#') {
            status = npbc_netpbm_skip_comment(reader);
            if (status != NPBC_STATUS_OK) {
                return status;
            }
            continue;
        }
        return NPBC_STATUS_OK;
    }
}

static NpbcStatus npbc_netpbm_read_uint32(
    NpbcNetpbmReader *reader,
    uint32_t maximum,
    uint32_t *value,
    int *terminator
) {
    uint32_t result;
    int character;
    NpbcStatus status;

    status = npbc_netpbm_next_token_start(reader, &character);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (character < '0' || character > '9') {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    result = 0;
    do {
        uint32_t digit = (uint32_t)(character - '0');

        if (result > (maximum - digit) / 10) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        result = result * 10 + digit;

        status = npbc_netpbm_read_character(reader, &character);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    } while (character >= '0' && character <= '9');

    if (
        character != EOF
        && !npbc_netpbm_is_whitespace(character)
        && character != '#'
    ) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (character == '#' && ungetc(character, reader->stream) == EOF) {
        return NPBC_STATUS_IO_FAILED;
    }

    *value = result;
    *terminator = character;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_read_magic(
    NpbcNetpbmReader *reader,
    int *format
) {
    int first;
    int second;
    int separator;
    NpbcStatus status;

    status = npbc_netpbm_read_character(reader, &first);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_netpbm_read_character(reader, &second);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_netpbm_read_character(reader, &separator);
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    if (first == EOF || second == EOF || separator == EOF) {
        return NPBC_STATUS_TRUNCATED_INPUT;
    }
    if (
        first != 'P'
        || second < '1'
        || second > '6'
        || !npbc_netpbm_is_whitespace(separator)
    ) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    *format = second - '0';
    return NPBC_STATUS_OK;
}

static NpbcImageKind npbc_netpbm_kind(int format) {
    if (format == 1 || format == 4) {
        return NPBC_IMAGE_BITMAP;
    }
    if (format == 2 || format == 5) {
        return NPBC_IMAGE_GRAYSCALE;
    }
    return NPBC_IMAGE_RGB;
}

static int npbc_netpbm_is_ascii(int format) {
    return format >= 1 && format <= 3;
}

static NpbcStatus npbc_netpbm_validate_binary_separator(int terminator) {
    if (terminator == EOF) {
        return NPBC_STATUS_TRUNCATED_INPUT;
    }
    if (!npbc_netpbm_is_whitespace(terminator)) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_finish_ascii(NpbcNetpbmReader *reader) {
    int character;
    NpbcStatus status;

    for (;;) {
        status = npbc_netpbm_read_character(reader, &character);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        if (character == EOF) {
            return NPBC_STATUS_OK;
        }
        if (npbc_netpbm_is_whitespace(character)) {
            continue;
        }
        if (character == '#') {
            status = npbc_netpbm_skip_comment(reader);
            if (status != NPBC_STATUS_OK) {
                return status;
            }
            continue;
        }
        return NPBC_STATUS_INVALID_FORMAT;
    }
}

static NpbcStatus npbc_netpbm_finish_binary(NpbcNetpbmReader *reader) {
    int character;
    NpbcStatus status;

    status = npbc_netpbm_read_character(reader, &character);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (character != EOF) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_read_ascii_bitmap(
    NpbcNetpbmReader *reader,
    NpbcImage *image
) {
    uint32_t row;
    uint32_t column;

    for (row = 0; row < image->height; row++) {
        for (column = 0; column < image->width; column++) {
            uint32_t sample;
            int terminator;
            size_t byte_index;
            unsigned int bit_index;
            NpbcStatus status = npbc_netpbm_read_uint32(
                reader,
                UINT32_MAX,
                &sample,
                &terminator
            );

            if (status != NPBC_STATUS_OK) {
                return status;
            }
            if (sample > 1) {
                return NPBC_STATUS_INVALID_FORMAT;
            }

            byte_index = (size_t)row * (((size_t)image->width + 7) / 8)
                + (size_t)column / 8;
            bit_index = 7U - (column % 8U);
            if (sample == 1) {
                image->raster[byte_index] |= (uint8_t)(1U << bit_index);
            }
        }
    }

    return npbc_netpbm_finish_ascii(reader);
}

static NpbcStatus npbc_netpbm_store_sample(
    NpbcImage *image,
    size_t sample_index,
    uint32_t sample
) {
    if (sample > image->max_value) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    if (image->max_value < 256) {
        image->raster[sample_index] = (uint8_t)sample;
    } else {
        npbc_write_u16_be(
            image->raster + sample_index * 2,
            (uint16_t)sample
        );
    }

    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_read_ascii_samples(
    NpbcNetpbmReader *reader,
    NpbcImage *image
) {
    uint64_t sample_count;
    uint64_t sample_index;

    sample_count = (uint64_t)image->width
        * (uint64_t)image->height
        * (uint64_t)image->channels;

    for (sample_index = 0; sample_index < sample_count; sample_index++) {
        uint32_t sample;
        int terminator;
        NpbcStatus status = npbc_netpbm_read_uint32(
            reader,
            UINT32_MAX,
            &sample,
            &terminator
        );

        if (status != NPBC_STATUS_OK) {
            return status;
        }
        status = npbc_netpbm_store_sample(
            image,
            (size_t)sample_index,
            sample
        );
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    }

    return npbc_netpbm_finish_ascii(reader);
}

static void npbc_netpbm_normalize_bitmap(NpbcImage *image) {
    size_t row_size;
    uint8_t mask;
    uint32_t row;
    unsigned int remainder;

    remainder = image->width % 8U;
    if (remainder == 0) {
        return;
    }

    row_size = ((size_t)image->width + 7) / 8;
    mask = (uint8_t)(UINT8_C(0xff) << (8U - remainder));
    for (row = 0; row < image->height; row++) {
        image->raster[(size_t)(row + 1) * row_size - 1] &= mask;
    }
}

static NpbcStatus npbc_netpbm_validate_binary_samples(
    const NpbcImage *image
) {
    size_t sample_size;
    size_t offset;

    if (image->kind == NPBC_IMAGE_BITMAP) {
        return NPBC_STATUS_OK;
    }

    sample_size = image->max_value < 256 ? 1 : 2;
    for (offset = 0; offset < image->raster_size; offset += sample_size) {
        uint32_t sample = sample_size == 1
            ? image->raster[offset]
            : npbc_read_u16_be(image->raster + offset);

        if (sample > image->max_value) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
    }

    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_read_binary(
    NpbcNetpbmReader *reader,
    NpbcImage *image
) {
    NpbcStatus status;

    status = npbc_read_exact(
        reader->stream,
        image->raster,
        image->raster_size
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    if (image->kind == NPBC_IMAGE_BITMAP) {
        npbc_netpbm_normalize_bitmap(image);
    } else {
        status = npbc_netpbm_validate_binary_samples(image);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    }

    return npbc_netpbm_finish_binary(reader);
}

static NpbcStatus npbc_netpbm_prepare_image(
    int format,
    uint32_t width,
    uint32_t height,
    uint16_t max_value,
    NpbcImage *image
) {
    NpbcStatus status;

    npbc_image_init(image);
    image->kind = npbc_netpbm_kind(format);
    image->width = width;
    image->height = height;
    image->channels = npbc_image_channels(image->kind);
    image->max_value = max_value;

    status = npbc_raster_size(
        image->kind,
        image->width,
        image->height,
        image->max_value,
        &image->raster_size
    );
    if (status != NPBC_STATUS_OK) {
        npbc_image_init(image);
        if (status == NPBC_STATUS_INVALID_ARGUMENT) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        return status;
    }

    image->raster = calloc(image->raster_size, 1);
    if (image->raster == NULL) {
        npbc_image_init(image);
        return NPBC_STATUS_ALLOCATION_FAILED;
    }

    return NPBC_STATUS_OK;
}

static int npbc_netpbm_image_is_empty(const NpbcImage *image) {
    return image->kind == 0
        && image->width == 0
        && image->height == 0
        && image->channels == 0
        && image->max_value == 0
        && image->raster == NULL
        && image->raster_size == 0;
}

NpbcStatus npbc_netpbm_read(FILE *stream, NpbcImage *image) {
    NpbcNetpbmReader reader;
    NpbcImage parsed;
    uint32_t width;
    uint32_t height;
    uint32_t max_value;
    int format;
    int terminator;
    NpbcStatus status;

    if (
        stream == NULL
        || image == NULL
        || !npbc_netpbm_image_is_empty(image)
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    reader.stream = stream;
    npbc_image_init(&parsed);

    status = npbc_netpbm_read_magic(&reader, &format);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_netpbm_read_uint32(
        &reader,
        UINT32_MAX,
        &width,
        &terminator
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_netpbm_read_uint32(
        &reader,
        UINT32_MAX,
        &height,
        &terminator
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    if (npbc_netpbm_kind(format) == NPBC_IMAGE_BITMAP) {
        max_value = 1;
    } else {
        status = npbc_netpbm_read_uint32(
            &reader,
            UINT16_MAX,
            &max_value,
            &terminator
        );
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        if (max_value == 0) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
    }

    if (!npbc_netpbm_is_ascii(format)) {
        status = npbc_netpbm_validate_binary_separator(terminator);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    }

    status = npbc_netpbm_prepare_image(
        format,
        width,
        height,
        (uint16_t)max_value,
        &parsed
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    if (format == 1) {
        status = npbc_netpbm_read_ascii_bitmap(&reader, &parsed);
    } else if (format == 2 || format == 3) {
        status = npbc_netpbm_read_ascii_samples(&reader, &parsed);
    } else {
        status = npbc_netpbm_read_binary(&reader, &parsed);
    }

    if (status != NPBC_STATUS_OK) {
        npbc_image_destroy(&parsed);
        return status;
    }

    *image = parsed;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_netpbm_validate_image(const NpbcImage *image) {
    size_t expected_size;
    size_t sample_size;
    size_t offset;
    uint8_t channels;
    NpbcStatus status;

    if (image == NULL || image->raster == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    channels = npbc_image_channels(image->kind);
    if (channels == 0 || image->channels != channels) {
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

static NpbcStatus npbc_netpbm_write_header(
    FILE *stream,
    const NpbcImage *image
) {
    char header[80];
    int length;
    int format;

    if (image->kind == NPBC_IMAGE_BITMAP) {
        format = 4;
        length = snprintf(
            header,
            sizeof(header),
            "P%d\n%" PRIu32 " %" PRIu32 "\n",
            format,
            image->width,
            image->height
        );
    } else {
        format = image->kind == NPBC_IMAGE_GRAYSCALE ? 5 : 6;
        length = snprintf(
            header,
            sizeof(header),
            "P%d\n%" PRIu32 " %" PRIu32 "\n%" PRIu16 "\n",
            format,
            image->width,
            image->height,
            image->max_value
        );
    }

    if (length < 0 || (size_t)length >= sizeof(header)) {
        return NPBC_STATUS_INTERNAL_ERROR;
    }

    return npbc_write_exact(stream, header, (size_t)length);
}

static NpbcStatus npbc_netpbm_write_bitmap(
    FILE *stream,
    const NpbcImage *image
) {
    size_t row_size;
    uint32_t row;
    unsigned int remainder;
    NpbcStatus status;

    row_size = ((size_t)image->width + 7) / 8;
    remainder = image->width % 8U;

    for (row = 0; row < image->height; row++) {
        const uint8_t *row_data = image->raster + (size_t)row * row_size;

        if (remainder == 0) {
            status = npbc_write_exact(stream, row_data, row_size);
        } else {
            uint8_t final_byte;
            uint8_t mask = (uint8_t)(UINT8_C(0xff) << (8U - remainder));

            status = npbc_write_exact(stream, row_data, row_size - 1);
            if (status != NPBC_STATUS_OK) {
                return status;
            }
            final_byte = row_data[row_size - 1] & mask;
            status = npbc_write_exact(stream, &final_byte, 1);
        }
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    }

    return NPBC_STATUS_OK;
}

NpbcStatus npbc_netpbm_write(FILE *stream, const NpbcImage *image) {
    NpbcStatus status;

    if (stream == NULL || image == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    status = npbc_netpbm_validate_image(image);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_netpbm_write_header(stream, image);
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    if (image->kind == NPBC_IMAGE_BITMAP) {
        return npbc_netpbm_write_bitmap(stream, image);
    }
    return npbc_write_exact(stream, image->raster, image->raster_size);
}
