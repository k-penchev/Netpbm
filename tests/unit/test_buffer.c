#include "test.h"

#include <stdint.h>
#include <string.h>

#include "npbc/buffer.h"

int npbc_test_buffer(void) {
    NpbcBuffer buffer;
    const uint8_t first[] = {1, 2, 3, 4};
    const uint8_t expected[] = {1, 2, 3, 4, 5};
    const uint8_t value = 9;
    size_t capacity;

    npbc_buffer_init(&buffer);
    NPBC_ASSERT(buffer.data == NULL);
    NPBC_ASSERT(buffer.size == 0);
    NPBC_ASSERT(buffer.capacity == 0);

    NPBC_ASSERT(
        npbc_buffer_append(&buffer, first, sizeof(first)) == NPBC_STATUS_OK
    );
    NPBC_ASSERT(npbc_buffer_append_byte(&buffer, 5) == NPBC_STATUS_OK);
    NPBC_ASSERT(buffer.size == sizeof(expected));
    NPBC_ASSERT_BYTES(buffer.data, expected, sizeof(expected));

    capacity = buffer.capacity;
    NPBC_ASSERT(npbc_buffer_reserve(&buffer, capacity + 100) == NPBC_STATUS_OK);
    NPBC_ASSERT(buffer.capacity >= capacity + 100);
    NPBC_ASSERT_BYTES(buffer.data, expected, sizeof(expected));

    npbc_buffer_reset(&buffer);
    NPBC_ASSERT(buffer.size == 0);
    NPBC_ASSERT(buffer.capacity >= capacity + 100);

    NPBC_ASSERT(npbc_buffer_append(&buffer, NULL, 0) == NPBC_STATUS_OK);
    NPBC_ASSERT(
        npbc_buffer_append(&buffer, NULL, 1)
            == NPBC_STATUS_INVALID_ARGUMENT
    );
    NPBC_ASSERT(
        npbc_buffer_reserve(NULL, 1) == NPBC_STATUS_INVALID_ARGUMENT
    );

    npbc_buffer_destroy(&buffer);
    NPBC_ASSERT(buffer.data == NULL);
    NPBC_ASSERT(buffer.size == 0);
    NPBC_ASSERT(buffer.capacity == 0);

    buffer.data = NULL;
    buffer.size = SIZE_MAX;
    buffer.capacity = SIZE_MAX;
    NPBC_ASSERT(
        npbc_buffer_append(&buffer, &value, 1) == NPBC_STATUS_SIZE_OVERFLOW
    );
    npbc_buffer_init(&buffer);

    npbc_buffer_destroy(&buffer);
    return 0;
}
