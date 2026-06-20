#ifndef NPBC_IMAGE_H
#define NPBC_IMAGE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    NPBC_IMAGE_BITMAP = 1,
    NPBC_IMAGE_GRAYSCALE = 2,
    NPBC_IMAGE_RGB = 3
} NpbcImageKind;

typedef struct {
    NpbcImageKind kind;
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint16_t max_value;
    uint8_t *raster;
    size_t raster_size;
} NpbcImage;

void npbc_image_init(NpbcImage *image);
void npbc_image_destroy(NpbcImage *image);
uint8_t npbc_image_channels(NpbcImageKind kind);

#endif
