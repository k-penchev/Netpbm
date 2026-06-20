#include "npbc/cli.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npbc/compression.h"
#include "npbc/container.h"
#include "npbc/image.h"
#include "npbc/netpbm.h"
#include "npbc/status.h"

typedef NpbcStatus (*NpbcCliWriter)(FILE *stream, const void *context);

typedef struct {
    const NpbcImage *image;
    NpbcCompressionAlgorithm algorithm;
} NpbcCliContainerWrite;

static void npbc_cli_help(void) {
    puts("Usage:");
    puts("  npbc compress --algorithm rle INPUT OUTPUT");
    puts("  npbc compress --algorithm huffman INPUT OUTPUT");
    puts("  npbc compress -a rle INPUT OUTPUT");
    puts("  npbc compress -a huffman INPUT OUTPUT");
    puts("  npbc decompress INPUT OUTPUT");
    puts("  npbc info INPUT");
    puts("  npbc verify INPUT");
    puts("  npbc --help");
}

int npbc_cli_exit_code(NpbcStatus status) {
    switch (status) {
        case NPBC_STATUS_OK:
            return 0;
        case NPBC_STATUS_INVALID_FORMAT:
        case NPBC_STATUS_UNSUPPORTED:
        case NPBC_STATUS_TRUNCATED_INPUT:
            return 3;
        case NPBC_STATUS_IO_FAILED:
        case NPBC_STATUS_OUTPUT_EXISTS:
            return 4;
        case NPBC_STATUS_INTEGRITY_FAILED:
            return 5;
        case NPBC_STATUS_SIZE_OVERFLOW:
        case NPBC_STATUS_ALLOCATION_FAILED:
            return 6;
        case NPBC_STATUS_INVALID_ARGUMENT:
        case NPBC_STATUS_INTERNAL_ERROR:
            return 7;
    }

    return 7;
}

static int npbc_cli_fail_status(const char *message, NpbcStatus status) {
    fprintf(stderr, "npbc: %s\n", message);
    return npbc_cli_exit_code(status);
}

static int npbc_cli_usage(const char *message) {
    fprintf(stderr, "npbc: %s\n", message);
    fputs("Try 'npbc --help'.\n", stderr);
    return 2;
}

static NpbcStatus npbc_cli_open_input(const char *path, FILE **stream) {
    FILE *opened;

    errno = 0;
    opened = fopen(path, "rb");
    if (opened == NULL) {
        return NPBC_STATUS_IO_FAILED;
    }
    *stream = opened;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_cli_close_input(FILE *stream, NpbcStatus status) {
    if (fclose(stream) != 0 && status == NPBC_STATUS_OK) {
        return NPBC_STATUS_IO_FAILED;
    }
    return status;
}

static NpbcStatus npbc_cli_file_size(FILE *stream, uint64_t *size) {
    long position;

    if (fseek(stream, 0, SEEK_END) != 0) {
        return NPBC_STATUS_IO_FAILED;
    }
    position = ftell(stream);
    if (position < 0) {
        return NPBC_STATUS_IO_FAILED;
    }
    if (fseek(stream, 0, SEEK_SET) != 0) {
        return NPBC_STATUS_IO_FAILED;
    }

    *size = (uint64_t)position;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_cli_destination_available(const char *path) {
    FILE *stream;

    errno = 0;
    stream = fopen(path, "rb");
    if (stream != NULL) {
        fclose(stream);
        return NPBC_STATUS_OUTPUT_EXISTS;
    }
    if (errno == ENOENT) {
        return NPBC_STATUS_OK;
    }
    return NPBC_STATUS_IO_FAILED;
}

static NpbcStatus npbc_cli_temp_path(
    const char *destination,
    unsigned int attempt,
    char **path
) {
    size_t destination_length;
    size_t capacity;
    char *candidate;
    int length;

    destination_length = strlen(destination);
    if (destination_length > SIZE_MAX - 32) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }
    capacity = destination_length + 32;
    candidate = malloc(capacity);
    if (candidate == NULL) {
        return NPBC_STATUS_ALLOCATION_FAILED;
    }
    length = snprintf(
        candidate,
        capacity,
        "%s.tmp.%u",
        destination,
        attempt
    );
    if (length < 0 || (size_t)length >= capacity) {
        free(candidate);
        return NPBC_STATUS_INTERNAL_ERROR;
    }

    *path = candidate;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_cli_open_temp(
    const char *destination,
    FILE **stream,
    char **temporary_path
) {
    unsigned int attempt;

    for (attempt = 0; attempt < 1000; attempt++) {
        char *path;
        NpbcStatus status = npbc_cli_temp_path(
            destination,
            attempt,
            &path
        );

        if (status != NPBC_STATUS_OK) {
            return status;
        }
        errno = 0;
        *stream = fopen(path, "wbx");
        if (*stream != NULL) {
            *temporary_path = path;
            return NPBC_STATUS_OK;
        }
        free(path);
        if (errno != EEXIST) {
            return NPBC_STATUS_IO_FAILED;
        }
    }

    return NPBC_STATUS_IO_FAILED;
}

static NpbcStatus npbc_cli_container_writer(
    FILE *stream,
    const void *context
) {
    const NpbcCliContainerWrite *request = context;

    return npbc_container_write(
        stream,
        request->image,
        request->algorithm
    );
}

static NpbcStatus npbc_cli_netpbm_writer(
    FILE *stream,
    const void *context
) {
    return npbc_netpbm_write(stream, context);
}

static NpbcStatus npbc_cli_write_transaction(
    const char *destination,
    NpbcCliWriter writer,
    const void *context,
    uint64_t *written_size
) {
    FILE *stream;
    char *temporary_path;
    NpbcStatus status;

    status = npbc_cli_destination_available(destination);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    stream = NULL;
    temporary_path = NULL;
    status = npbc_cli_open_temp(
        destination,
        &stream,
        &temporary_path
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    status = writer(stream, context);
    if (status == NPBC_STATUS_OK && fflush(stream) != 0) {
        status = NPBC_STATUS_IO_FAILED;
    }
    if (status == NPBC_STATUS_OK && written_size != NULL) {
        long position = ftell(stream);

        if (position < 0) {
            status = NPBC_STATUS_IO_FAILED;
        } else {
            *written_size = (uint64_t)position;
        }
    }
    if (fclose(stream) != 0 && status == NPBC_STATUS_OK) {
        status = NPBC_STATUS_IO_FAILED;
    }

    if (status == NPBC_STATUS_OK) {
        status = npbc_cli_destination_available(destination);
    }
    if (
        status == NPBC_STATUS_OK
        && rename(temporary_path, destination) != 0
    ) {
        status = NPBC_STATUS_IO_FAILED;
    }
    if (status != NPBC_STATUS_OK) {
        remove(temporary_path);
    }

    free(temporary_path);
    return status;
}

static int npbc_cli_algorithm(
    const char *name,
    NpbcCompressionAlgorithm *algorithm
) {
    if (strcmp(name, "rle") == 0) {
        *algorithm = NPBC_COMPRESSION_RLE;
        return 1;
    }
    if (strcmp(name, "huffman") == 0) {
        *algorithm = NPBC_COMPRESSION_HUFFMAN;
        return 1;
    }
    return 0;
}

static const char *npbc_cli_image_name(NpbcImageKind kind) {
    switch (kind) {
        case NPBC_IMAGE_BITMAP:
            return "bitmap";
        case NPBC_IMAGE_GRAYSCALE:
            return "grayscale";
        case NPBC_IMAGE_RGB:
            return "rgb";
    }

    return "unknown";
}

static int npbc_cli_compress(
    NpbcCompressionAlgorithm algorithm,
    const char *input_path,
    const char *output_path
) {
    FILE *input;
    NpbcImage image;
    NpbcCliContainerWrite request;
    uint64_t input_size;
    uint64_t output_size;
    NpbcStatus status;

    input = NULL;
    npbc_image_init(&image);
    status = npbc_cli_open_input(input_path, &input);
    if (status == NPBC_STATUS_OK) {
        status = npbc_cli_file_size(input, &input_size);
    }
    if (status == NPBC_STATUS_OK) {
        status = npbc_netpbm_read(input, &image);
    }
    if (input != NULL) {
        status = npbc_cli_close_input(input, status);
    }
    if (status != NPBC_STATUS_OK) {
        npbc_image_destroy(&image);
        return npbc_cli_fail_status(
            "could not read Netpbm input",
            status
        );
    }

    request.image = &image;
    request.algorithm = algorithm;
    status = npbc_cli_write_transaction(
        output_path,
        npbc_cli_container_writer,
        &request,
        &output_size
    );
    npbc_image_destroy(&image);
    if (status != NPBC_STATUS_OK) {
        return npbc_cli_fail_status(
            status == NPBC_STATUS_OUTPUT_EXISTS
                ? "output file already exists"
                : "could not write compressed output",
            status
        );
    }

    printf("algorithm: %s\n", npbc_compression_algorithm_name(algorithm));
    printf("input-bytes: %" PRIu64 "\n", input_size);
    printf("output-bytes: %" PRIu64 "\n", output_size);
    printf(
        "ratio: %.4f\n",
        input_size == 0 ? 0.0 : (double)output_size / (double)input_size
    );
    return 0;
}

static int npbc_cli_decompress(
    const char *input_path,
    const char *output_path
) {
    FILE *input;
    NpbcImage image;
    NpbcStatus status;

    input = NULL;
    npbc_image_init(&image);
    status = npbc_cli_open_input(input_path, &input);
    if (status == NPBC_STATUS_OK) {
        status = npbc_container_read(input, &image, NULL);
    }
    if (input != NULL) {
        status = npbc_cli_close_input(input, status);
    }
    if (status != NPBC_STATUS_OK) {
        npbc_image_destroy(&image);
        return npbc_cli_fail_status(
            status == NPBC_STATUS_INTEGRITY_FAILED
                ? "compressed input failed integrity verification"
                : "could not read compressed input",
            status
        );
    }

    status = npbc_cli_write_transaction(
        output_path,
        npbc_cli_netpbm_writer,
        &image,
        NULL
    );
    npbc_image_destroy(&image);
    if (status != NPBC_STATUS_OK) {
        return npbc_cli_fail_status(
            status == NPBC_STATUS_OUTPUT_EXISTS
                ? "output file already exists"
                : "could not write Netpbm output",
            status
        );
    }
    return 0;
}

static int npbc_cli_info(const char *input_path) {
    FILE *input;
    NpbcContainerInfo info;
    NpbcStatus status;

    input = NULL;
    status = npbc_cli_open_input(input_path, &input);
    if (status == NPBC_STATUS_OK) {
        status = npbc_container_inspect(input, &info);
    }
    if (input != NULL) {
        status = npbc_cli_close_input(input, status);
    }
    if (status != NPBC_STATUS_OK) {
        return npbc_cli_fail_status(
            "could not inspect compressed input",
            status
        );
    }

    printf("compatibility: %u\n", info.compatibility);
    printf(
        "algorithm: %s\n",
        npbc_compression_algorithm_name(info.algorithm)
    );
    printf("image: %s\n", npbc_cli_image_name(info.image_kind));
    printf("width: %" PRIu32 "\n", info.width);
    printf("height: %" PRIu32 "\n", info.height);
    printf("max-value: %" PRIu16 "\n", info.max_value);
    printf("raster-bytes: %" PRIu64 "\n", info.raster_size);
    printf("payload-bytes: %" PRIu64 "\n", info.payload_size);
    printf(
        "ratio: %.4f\n",
        (double)info.payload_size / (double)info.raster_size
    );
    puts("ratio-basis: payload/raster");
    return 0;
}

static int npbc_cli_verify(const char *input_path) {
    FILE *input;
    NpbcImage image;
    NpbcStatus status;

    input = NULL;
    npbc_image_init(&image);
    status = npbc_cli_open_input(input_path, &input);
    if (status == NPBC_STATUS_OK) {
        status = npbc_container_read(input, &image, NULL);
    }
    if (input != NULL) {
        status = npbc_cli_close_input(input, status);
    }
    npbc_image_destroy(&image);
    if (status != NPBC_STATUS_OK) {
        return npbc_cli_fail_status(
            status == NPBC_STATUS_INTEGRITY_FAILED
                ? "compressed input failed integrity verification"
                : "compressed input is invalid",
            status
        );
    }

    puts("valid");
    return 0;
}

int npbc_cli_run(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        npbc_cli_help();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "compress") == 0) {
        NpbcCompressionAlgorithm algorithm;

        if (argc != 6) {
            return npbc_cli_usage(
                "compress requires an algorithm, input, and output"
            );
        }
        if (
            strcmp(argv[2], "--algorithm") != 0
            && strcmp(argv[2], "-a") != 0
        ) {
            return npbc_cli_usage(
                "compress requires --algorithm or -a before the algorithm"
            );
        }
        if (!npbc_cli_algorithm(argv[3], &algorithm)) {
            return npbc_cli_usage("unknown compression algorithm");
        }
        return npbc_cli_compress(algorithm, argv[4], argv[5]);
    }
    if (argc >= 2 && strcmp(argv[1], "decompress") == 0) {
        if (argc != 4) {
            return npbc_cli_usage(
                "decompress requires an input and output"
            );
        }
        return npbc_cli_decompress(argv[2], argv[3]);
    }
    if (argc >= 2 && strcmp(argv[1], "info") == 0) {
        if (argc != 3) {
            return npbc_cli_usage("info requires one input");
        }
        return npbc_cli_info(argv[2]);
    }
    if (argc >= 2 && strcmp(argv[1], "verify") == 0) {
        if (argc != 3) {
            return npbc_cli_usage("verify requires one input");
        }
        return npbc_cli_verify(argv[2]);
    }

    return npbc_cli_usage("unknown command");
}
