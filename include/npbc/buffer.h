#ifndef NPBC_BUFFER_H
#define NPBC_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#include "npbc/status.h"

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} NpbcBuffer;

void npbc_buffer_init(NpbcBuffer *buffer);
void npbc_buffer_reset(NpbcBuffer *buffer);
void npbc_buffer_destroy(NpbcBuffer *buffer);
NpbcStatus npbc_buffer_reserve(NpbcBuffer *buffer, size_t capacity);
NpbcStatus npbc_buffer_append(
    NpbcBuffer *buffer,
    const void *data,
    size_t size
);
NpbcStatus npbc_buffer_append_byte(NpbcBuffer *buffer, uint8_t value);

#endif
