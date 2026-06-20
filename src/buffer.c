#include "npbc/buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static NpbcStatus npbc_buffer_required_capacity(
    size_t current,
    size_t required,
    size_t *result
) {
    size_t capacity;

    if (result == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }

    capacity = current == 0 ? 64 : current;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }

    *result = capacity;
    return NPBC_STATUS_OK;
}

void npbc_buffer_init(NpbcBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

void npbc_buffer_reset(NpbcBuffer *buffer) {
    if (buffer != NULL) {
        buffer->size = 0;
    }
}

void npbc_buffer_destroy(NpbcBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    free(buffer->data);
    npbc_buffer_init(buffer);
}

NpbcStatus npbc_buffer_reserve(NpbcBuffer *buffer, size_t capacity) {
    uint8_t *data;

    if (buffer == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (capacity <= buffer->capacity) {
        return NPBC_STATUS_OK;
    }

    data = realloc(buffer->data, capacity);
    if (data == NULL) {
        return NPBC_STATUS_ALLOCATION_FAILED;
    }

    buffer->data = data;
    buffer->capacity = capacity;
    return NPBC_STATUS_OK;
}

NpbcStatus npbc_buffer_append(
    NpbcBuffer *buffer,
    const void *data,
    size_t size
) {
    size_t required;
    size_t capacity;
    NpbcStatus status;

    if (buffer == NULL || (data == NULL && size != 0)) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (size == 0) {
        return NPBC_STATUS_OK;
    }
    if (buffer->size > SIZE_MAX - size) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    required = buffer->size + size;
    if (required > buffer->capacity) {
        status = npbc_buffer_required_capacity(
            buffer->capacity,
            required,
            &capacity
        );
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        status = npbc_buffer_reserve(buffer, capacity);
        if (status != NPBC_STATUS_OK) {
            return status;
        }
    }

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size = required;
    return NPBC_STATUS_OK;
}

NpbcStatus npbc_buffer_append_byte(NpbcBuffer *buffer, uint8_t value) {
    return npbc_buffer_append(buffer, &value, 1);
}
