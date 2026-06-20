#include "test.h"

#include <stdint.h>
#include <stdlib.h>

#include "npbc/image.h"

int npbc_test_image(void) {
    NpbcImage image;

    npbc_image_init(&image);
    NPBC_ASSERT(image.kind == 0);
    NPBC_ASSERT(image.raster == NULL);
    NPBC_ASSERT(image.raster_size == 0);

    NPBC_ASSERT(npbc_image_channels(NPBC_IMAGE_BITMAP) == 1);
    NPBC_ASSERT(npbc_image_channels(NPBC_IMAGE_GRAYSCALE) == 1);
    NPBC_ASSERT(npbc_image_channels(NPBC_IMAGE_RGB) == 3);
    NPBC_ASSERT(npbc_image_channels((NpbcImageKind)0) == 0);

    image.kind = NPBC_IMAGE_RGB;
    image.width = 2;
    image.height = 3;
    image.channels = 3;
    image.max_value = 255;
    image.raster_size = 18;
    image.raster = malloc(image.raster_size);
    NPBC_ASSERT(image.raster != NULL);

    npbc_image_destroy(&image);
    NPBC_ASSERT(image.kind == 0);
    NPBC_ASSERT(image.width == 0);
    NPBC_ASSERT(image.height == 0);
    NPBC_ASSERT(image.channels == 0);
    NPBC_ASSERT(image.max_value == 0);
    NPBC_ASSERT(image.raster == NULL);
    NPBC_ASSERT(image.raster_size == 0);

    npbc_image_destroy(&image);
    return 0;
}
