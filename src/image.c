#include "npbc/image.h"

#include <stdlib.h>

void npbc_image_init(NpbcImage *image) {
    if (image == NULL) {
        return;
    }

    image->kind = 0;
    image->width = 0;
    image->height = 0;
    image->channels = 0;
    image->max_value = 0;
    image->raster = NULL;
    image->raster_size = 0;
}

void npbc_image_destroy(NpbcImage *image) {
    if (image == NULL) {
        return;
    }

    free(image->raster);
    npbc_image_init(image);
}

uint8_t npbc_image_channels(NpbcImageKind kind) {
    switch (kind) {
        case NPBC_IMAGE_BITMAP:
        case NPBC_IMAGE_GRAYSCALE:
            return 1;
        case NPBC_IMAGE_RGB:
            return 3;
    }

    return 0;
}
