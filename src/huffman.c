#include "npbc/huffman.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "npbc/util.h"

#define NPBC_HUFFMAN_SYMBOL_COUNT 256
#define NPBC_HUFFMAN_NODE_COUNT 511

typedef struct {
    uint64_t frequency;
    uint16_t minimum_symbol;
    uint16_t creation_order;
    int16_t left;
    int16_t right;
    int16_t symbol;
    uint8_t active;
} NpbcHuffmanNode;

typedef struct {
    uint8_t symbol;
    uint8_t length;
    uint64_t code;
} NpbcHuffmanEntry;

typedef struct {
    uint16_t count[NPBC_HUFFMAN_MAX_CODE_LENGTH + 1];
    uint16_t first_index[NPBC_HUFFMAN_MAX_CODE_LENGTH + 1];
    uint64_t first_code[NPBC_HUFFMAN_MAX_CODE_LENGTH + 1];
    uint8_t symbols[NPBC_HUFFMAN_SYMBOL_COUNT];
    uint8_t maximum_length;
    uint16_t symbol_count;
} NpbcHuffmanTable;

static int npbc_huffman_buffer_is_empty(const NpbcBuffer *buffer) {
    return buffer != NULL
        && buffer->data == NULL
        && buffer->size == 0
        && buffer->capacity == 0;
}

static int npbc_huffman_node_less(
    const NpbcHuffmanNode *left,
    const NpbcHuffmanNode *right
) {
    if (left->frequency != right->frequency) {
        return left->frequency < right->frequency;
    }
    if (left->minimum_symbol != right->minimum_symbol) {
        return left->minimum_symbol < right->minimum_symbol;
    }
    return left->creation_order < right->creation_order;
}

static int npbc_huffman_select_node(
    const NpbcHuffmanNode nodes[NPBC_HUFFMAN_NODE_COUNT],
    size_t node_count
) {
    size_t index;
    int selected;

    selected = -1;
    for (index = 0; index < node_count; index++) {
        if (
            nodes[index].active
            && (
                selected < 0
                || npbc_huffman_node_less(
                    &nodes[index],
                    &nodes[(size_t)selected]
                )
            )
        ) {
            selected = (int)index;
        }
    }

    return selected;
}

static NpbcStatus npbc_huffman_assign_tree_lengths(
    const NpbcHuffmanNode nodes[NPBC_HUFFMAN_NODE_COUNT],
    int node_index,
    unsigned int depth,
    uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT]
) {
    const NpbcHuffmanNode *node = &nodes[(size_t)node_index];
    NpbcStatus status;

    if (node->symbol >= 0) {
        if (depth == 0 || depth > NPBC_HUFFMAN_MAX_CODE_LENGTH) {
            return NPBC_STATUS_INTERNAL_ERROR;
        }
        lengths[(size_t)node->symbol] = (uint8_t)depth;
        return NPBC_STATUS_OK;
    }
    if (
        node->left < 0
        || node->right < 0
        || depth >= NPBC_HUFFMAN_MAX_CODE_LENGTH
    ) {
        return NPBC_STATUS_INTERNAL_ERROR;
    }

    status = npbc_huffman_assign_tree_lengths(
        nodes,
        node->left,
        depth + 1,
        lengths
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    return npbc_huffman_assign_tree_lengths(
        nodes,
        node->right,
        depth + 1,
        lengths
    );
}

static NpbcStatus npbc_huffman_build_lengths(
    const uint64_t frequencies[NPBC_HUFFMAN_SYMBOL_COUNT],
    uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT]
) {
    NpbcHuffmanNode nodes[NPBC_HUFFMAN_NODE_COUNT];
    size_t node_count;
    size_t symbol;
    size_t active_count;

    memset(nodes, 0, sizeof(nodes));
    memset(lengths, 0, NPBC_HUFFMAN_SYMBOL_COUNT);
    node_count = 0;
    active_count = 0;

    for (symbol = 0; symbol < NPBC_HUFFMAN_SYMBOL_COUNT; symbol++) {
        if (frequencies[symbol] != 0) {
            nodes[node_count].frequency = frequencies[symbol];
            nodes[node_count].minimum_symbol = (uint16_t)symbol;
            nodes[node_count].creation_order = (uint16_t)node_count;
            nodes[node_count].left = -1;
            nodes[node_count].right = -1;
            nodes[node_count].symbol = (int16_t)symbol;
            nodes[node_count].active = 1;
            node_count++;
            active_count++;
        }
    }

    if (active_count == 0) {
        return NPBC_STATUS_OK;
    }
    if (active_count == 1) {
        lengths[nodes[0].minimum_symbol] = 1;
        return NPBC_STATUS_OK;
    }

    while (active_count > 1) {
        int left_index;
        int right_index;
        NpbcHuffmanNode *parent;

        left_index = npbc_huffman_select_node(nodes, node_count);
        if (left_index < 0) {
            return NPBC_STATUS_INTERNAL_ERROR;
        }
        nodes[(size_t)left_index].active = 0;

        right_index = npbc_huffman_select_node(nodes, node_count);
        if (right_index < 0 || node_count >= NPBC_HUFFMAN_NODE_COUNT) {
            return NPBC_STATUS_INTERNAL_ERROR;
        }
        nodes[(size_t)right_index].active = 0;

        parent = &nodes[node_count];
        parent->frequency = nodes[(size_t)left_index].frequency
            + nodes[(size_t)right_index].frequency;
        parent->minimum_symbol =
            nodes[(size_t)left_index].minimum_symbol
                < nodes[(size_t)right_index].minimum_symbol
            ? nodes[(size_t)left_index].minimum_symbol
            : nodes[(size_t)right_index].minimum_symbol;
        parent->creation_order = (uint16_t)node_count;
        parent->left = (int16_t)left_index;
        parent->right = (int16_t)right_index;
        parent->symbol = -1;
        parent->active = 1;
        node_count++;
        active_count--;
    }

    return npbc_huffman_assign_tree_lengths(
        nodes,
        npbc_huffman_select_node(nodes, node_count),
        0,
        lengths
    );
}

static int npbc_huffman_entry_less(
    const NpbcHuffmanEntry *left,
    const NpbcHuffmanEntry *right
) {
    if (left->length != right->length) {
        return left->length < right->length;
    }
    return left->symbol < right->symbol;
}

static void npbc_huffman_sort_entries(
    NpbcHuffmanEntry entries[NPBC_HUFFMAN_SYMBOL_COUNT],
    size_t entry_count
) {
    size_t index;

    for (index = 1; index < entry_count; index++) {
        NpbcHuffmanEntry entry = entries[index];
        size_t position = index;

        while (
            position > 0
            && npbc_huffman_entry_less(&entry, &entries[position - 1])
        ) {
            entries[position] = entries[position - 1];
            position--;
        }
        entries[position] = entry;
    }
}

static NpbcStatus npbc_huffman_validate_lengths(
    const uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT],
    size_t expected_size,
    uint16_t counts[NPBC_HUFFMAN_MAX_CODE_LENGTH + 1],
    uint16_t *symbol_count,
    uint8_t *maximum_length
) {
    uint64_t available;
    size_t symbol;
    unsigned int length;

    memset(
        counts,
        0,
        sizeof(uint16_t) * (NPBC_HUFFMAN_MAX_CODE_LENGTH + 1)
    );
    *symbol_count = 0;
    *maximum_length = 0;

    for (symbol = 0; symbol < NPBC_HUFFMAN_SYMBOL_COUNT; symbol++) {
        length = lengths[symbol];
        if (length > NPBC_HUFFMAN_MAX_CODE_LENGTH) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        if (length != 0) {
            counts[length]++;
            (*symbol_count)++;
            if (length > *maximum_length) {
                *maximum_length = (uint8_t)length;
            }
        }
    }

    if (expected_size == 0) {
        return *symbol_count == 0
            ? NPBC_STATUS_OK
            : NPBC_STATUS_INVALID_FORMAT;
    }
    if (*symbol_count == 0) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (*symbol_count == 1) {
        return counts[1] == 1
            ? NPBC_STATUS_OK
            : NPBC_STATUS_INVALID_FORMAT;
    }

    available = 1;
    for (length = 1; length <= NPBC_HUFFMAN_MAX_CODE_LENGTH; length++) {
        if (available > UINT64_MAX / 2) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        available *= 2;
        if (counts[length] > available) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        available -= counts[length];
    }

    return available == 0
        ? NPBC_STATUS_OK
        : NPBC_STATUS_INVALID_FORMAT;
}

static NpbcStatus npbc_huffman_build_entries(
    const uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT],
    size_t expected_size,
    NpbcHuffmanEntry entries[NPBC_HUFFMAN_SYMBOL_COUNT],
    size_t *entry_count
) {
    uint16_t counts[NPBC_HUFFMAN_MAX_CODE_LENGTH + 1];
    uint16_t symbol_count;
    uint8_t maximum_length;
    size_t symbol;
    size_t index;
    uint64_t code;
    uint8_t previous_length;
    NpbcStatus status;

    status = npbc_huffman_validate_lengths(
        lengths,
        expected_size,
        counts,
        &symbol_count,
        &maximum_length
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    index = 0;
    for (symbol = 0; symbol < NPBC_HUFFMAN_SYMBOL_COUNT; symbol++) {
        if (lengths[symbol] != 0) {
            entries[index].symbol = (uint8_t)symbol;
            entries[index].length = lengths[symbol];
            entries[index].code = 0;
            index++;
        }
    }
    *entry_count = index;
    npbc_huffman_sort_entries(entries, index);
    if (index == 0) {
        return NPBC_STATUS_OK;
    }

    code = 0;
    previous_length = entries[0].length;
    entries[0].code = 0;
    for (index = 1; index < *entry_count; index++) {
        unsigned int shift = entries[index].length - previous_length;

        if (code == UINT64_MAX) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        code++;
        if (shift != 0) {
            if (code > (UINT64_MAX >> shift)) {
                return NPBC_STATUS_INVALID_FORMAT;
            }
            code <<= shift;
        }
        if (code >= (UINT64_C(1) << entries[index].length)) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
        entries[index].code = code;
        previous_length = entries[index].length;
    }

    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_huffman_calculate_bit_count(
    const uint64_t frequencies[NPBC_HUFFMAN_SYMBOL_COUNT],
    const uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT],
    uint64_t *bit_count
) {
    uint64_t total;
    size_t symbol;

    total = 0;
    for (symbol = 0; symbol < NPBC_HUFFMAN_SYMBOL_COUNT; symbol++) {
        uint64_t contribution;

        contribution = frequencies[symbol] * lengths[symbol];
        if (total > UINT64_MAX - contribution) {
            return NPBC_STATUS_SIZE_OVERFLOW;
        }
        total += contribution;
    }

    *bit_count = total;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_huffman_payload_size(
    uint64_t bit_count,
    size_t *payload_size
) {
    uint64_t byte_count;

    byte_count = bit_count / 8;
    if (bit_count % 8 != 0) {
        byte_count++;
    }
    if (byte_count > SIZE_MAX) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    *payload_size = (size_t)byte_count;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_huffman_prepare_metadata(
    uint64_t bit_count,
    const uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT],
    NpbcBuffer *metadata
) {
    NpbcStatus status;

    status = npbc_buffer_reserve(metadata, NPBC_HUFFMAN_METADATA_SIZE);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    npbc_write_u64_be(metadata->data, bit_count);
    memcpy(metadata->data + 8, lengths, NPBC_HUFFMAN_SYMBOL_COUNT);
    metadata->size = NPBC_HUFFMAN_METADATA_SIZE;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_huffman_prepare_payload(
    size_t payload_size,
    NpbcBuffer *payload
) {
    NpbcStatus status;

    if (payload_size == 0) {
        return NPBC_STATUS_OK;
    }
    status = npbc_buffer_reserve(payload, payload_size);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    memset(payload->data, 0, payload_size);
    payload->size = payload_size;
    return NPBC_STATUS_OK;
}

static void npbc_huffman_write_code(
    NpbcBuffer *payload,
    uint64_t *bit_offset,
    uint64_t code,
    uint8_t length
) {
    unsigned int index;

    for (index = length; index > 0; index--) {
        uint8_t bit = (uint8_t)((code >> (index - 1)) & UINT64_C(1));
        size_t byte_index = (size_t)(*bit_offset / 8);
        unsigned int bit_index = 7U - (unsigned int)(*bit_offset % 8);

        payload->data[byte_index] |= (uint8_t)(bit << bit_index);
        (*bit_offset)++;
    }
}

static void npbc_huffman_map_codes(
    const NpbcHuffmanEntry entries[NPBC_HUFFMAN_SYMBOL_COUNT],
    size_t entry_count,
    uint64_t codes[NPBC_HUFFMAN_SYMBOL_COUNT]
) {
    size_t index;

    memset(codes, 0, sizeof(uint64_t) * NPBC_HUFFMAN_SYMBOL_COUNT);
    for (index = 0; index < entry_count; index++) {
        codes[entries[index].symbol] = entries[index].code;
    }
}

NpbcStatus npbc_huffman_encode(
    const uint8_t *input,
    size_t input_size,
    NpbcBuffer *metadata,
    NpbcBuffer *payload
) {
    uint64_t frequencies[NPBC_HUFFMAN_SYMBOL_COUNT];
    uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT];
    NpbcHuffmanEntry entries[NPBC_HUFFMAN_SYMBOL_COUNT];
    uint64_t codes[NPBC_HUFFMAN_SYMBOL_COUNT];
    NpbcBuffer encoded_metadata;
    NpbcBuffer encoded_payload;
    uint64_t bit_count;
    uint64_t bit_offset;
    size_t payload_size;
    size_t entry_count;
    size_t index;
    NpbcStatus status;

    if (
        (input == NULL && input_size != 0)
        || !npbc_huffman_buffer_is_empty(metadata)
        || !npbc_huffman_buffer_is_empty(payload)
        || metadata == payload
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (input_size > NPBC_MAX_RASTER_SIZE) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    memset(frequencies, 0, sizeof(frequencies));
    for (index = 0; index < input_size; index++) {
        frequencies[input[index]]++;
    }

    status = npbc_huffman_build_lengths(frequencies, lengths);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_huffman_build_entries(
        lengths,
        input_size,
        entries,
        &entry_count
    );
    if (status != NPBC_STATUS_OK) {
        return status == NPBC_STATUS_INVALID_FORMAT
            ? NPBC_STATUS_INTERNAL_ERROR
            : status;
    }
    status = npbc_huffman_calculate_bit_count(
        frequencies,
        lengths,
        &bit_count
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_huffman_payload_size(bit_count, &payload_size);
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    npbc_buffer_init(&encoded_metadata);
    npbc_buffer_init(&encoded_payload);
    status = npbc_huffman_prepare_metadata(
        bit_count,
        lengths,
        &encoded_metadata
    );
    if (status != NPBC_STATUS_OK) {
        npbc_buffer_destroy(&encoded_metadata);
        return status;
    }
    status = npbc_huffman_prepare_payload(payload_size, &encoded_payload);
    if (status != NPBC_STATUS_OK) {
        npbc_buffer_destroy(&encoded_metadata);
        npbc_buffer_destroy(&encoded_payload);
        return status;
    }

    npbc_huffman_map_codes(entries, entry_count, codes);
    bit_offset = 0;
    for (index = 0; index < input_size; index++) {
        npbc_huffman_write_code(
            &encoded_payload,
            &bit_offset,
            codes[input[index]],
            lengths[input[index]]
        );
    }
    if (bit_offset != bit_count) {
        npbc_buffer_destroy(&encoded_metadata);
        npbc_buffer_destroy(&encoded_payload);
        return NPBC_STATUS_INTERNAL_ERROR;
    }

    *metadata = encoded_metadata;
    *payload = encoded_payload;
    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_huffman_build_table(
    const uint8_t lengths[NPBC_HUFFMAN_SYMBOL_COUNT],
    size_t expected_size,
    NpbcHuffmanTable *table
) {
    NpbcHuffmanEntry entries[NPBC_HUFFMAN_SYMBOL_COUNT];
    size_t entry_count;
    size_t index;
    unsigned int length;
    NpbcStatus status;

    memset(table, 0, sizeof(*table));
    status = npbc_huffman_build_entries(
        lengths,
        expected_size,
        entries,
        &entry_count
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }

    table->symbol_count = (uint16_t)entry_count;
    for (index = 0; index < entry_count; index++) {
        length = entries[index].length;
        if (table->count[length] == 0) {
            table->first_index[length] = (uint16_t)index;
            table->first_code[length] = entries[index].code;
        }
        table->count[length]++;
        table->symbols[index] = entries[index].symbol;
        if (length > table->maximum_length) {
            table->maximum_length = (uint8_t)length;
        }
    }

    return NPBC_STATUS_OK;
}

static NpbcStatus npbc_huffman_validate_stream_shape(
    uint64_t bit_count,
    const uint8_t *payload,
    size_t payload_size
) {
    size_t expected_payload_size;
    NpbcStatus status;

    status = npbc_huffman_payload_size(bit_count, &expected_payload_size);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (payload_size != expected_payload_size) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (bit_count % 8 != 0 && payload_size != 0) {
        unsigned int padding = 8U - (unsigned int)(bit_count % 8);
        uint8_t mask = (uint8_t)((UINT8_C(1) << padding) - 1);

        if ((payload[payload_size - 1] & mask) != 0) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
    }

    return NPBC_STATUS_OK;
}

NpbcStatus npbc_huffman_validate_metadata(
    const uint8_t *metadata,
    size_t metadata_size,
    size_t expected_size,
    uint64_t *bit_count,
    size_t *payload_size
) {
    NpbcHuffmanTable table;
    uint64_t parsed_bit_count;
    size_t parsed_payload_size;
    NpbcStatus status;

    if (metadata == NULL || bit_count == NULL || payload_size == NULL) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (metadata_size != NPBC_HUFFMAN_METADATA_SIZE) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (expected_size > NPBC_MAX_RASTER_SIZE) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    parsed_bit_count = npbc_read_u64_be(metadata);
    status = npbc_huffman_payload_size(
        parsed_bit_count,
        &parsed_payload_size
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_huffman_build_table(
        metadata + 8,
        expected_size,
        &table
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (expected_size == 0 && parsed_bit_count != 0) {
        return NPBC_STATUS_INVALID_FORMAT;
    }

    *bit_count = parsed_bit_count;
    *payload_size = parsed_payload_size;
    return NPBC_STATUS_OK;
}

static uint8_t npbc_huffman_read_bit(
    const uint8_t *payload,
    uint64_t bit_offset
) {
    size_t byte_index = (size_t)(bit_offset / 8);
    unsigned int bit_index = 7U - (unsigned int)(bit_offset % 8);

    return (uint8_t)((payload[byte_index] >> bit_index) & UINT8_C(1));
}

static NpbcStatus npbc_huffman_decode_symbol(
    const NpbcHuffmanTable *table,
    const uint8_t *payload,
    uint64_t bit_count,
    uint64_t *bit_offset,
    uint8_t *symbol
) {
    uint64_t code;
    unsigned int length;

    code = 0;
    for (length = 1; length <= table->maximum_length; length++) {
        uint64_t first_code;
        uint64_t offset;

        if (*bit_offset >= bit_count) {
            return NPBC_STATUS_TRUNCATED_INPUT;
        }
        code = (code << 1) | npbc_huffman_read_bit(payload, *bit_offset);
        (*bit_offset)++;

        if (table->count[length] == 0) {
            continue;
        }
        first_code = table->first_code[length];
        if (code < first_code) {
            continue;
        }
        offset = code - first_code;
        if (offset < table->count[length]) {
            *symbol = table->symbols[table->first_index[length] + offset];
            return NPBC_STATUS_OK;
        }
    }

    return NPBC_STATUS_INVALID_FORMAT;
}

NpbcStatus npbc_huffman_decode(
    const uint8_t *metadata,
    size_t metadata_size,
    const uint8_t *payload,
    size_t payload_size,
    size_t expected_size,
    NpbcBuffer *output
) {
    NpbcHuffmanTable table;
    NpbcBuffer decoded;
    uint64_t bit_count;
    uint64_t bit_offset;
    size_t output_index;
    NpbcStatus status;

    if (
        metadata == NULL
        || (payload == NULL && payload_size != 0)
        || !npbc_huffman_buffer_is_empty(output)
    ) {
        return NPBC_STATUS_INVALID_ARGUMENT;
    }
    if (metadata_size != NPBC_HUFFMAN_METADATA_SIZE) {
        return NPBC_STATUS_INVALID_FORMAT;
    }
    if (expected_size > NPBC_MAX_RASTER_SIZE) {
        return NPBC_STATUS_SIZE_OVERFLOW;
    }

    {
        size_t expected_payload_size;

        status = npbc_huffman_validate_metadata(
            metadata,
            metadata_size,
            expected_size,
            &bit_count,
            &expected_payload_size
        );
        if (status != NPBC_STATUS_OK) {
            return status;
        }
        if (payload_size != expected_payload_size) {
            return NPBC_STATUS_INVALID_FORMAT;
        }
    }
    status = npbc_huffman_validate_stream_shape(
        bit_count,
        payload,
        payload_size
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    status = npbc_huffman_build_table(
        metadata + 8,
        expected_size,
        &table
    );
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    if (expected_size == 0) {
        return bit_count == 0
            ? NPBC_STATUS_OK
            : NPBC_STATUS_INVALID_FORMAT;
    }

    npbc_buffer_init(&decoded);
    status = npbc_buffer_reserve(&decoded, expected_size);
    if (status != NPBC_STATUS_OK) {
        return status;
    }
    decoded.size = expected_size;

    bit_offset = 0;
    for (output_index = 0; output_index < expected_size; output_index++) {
        status = npbc_huffman_decode_symbol(
            &table,
            payload,
            bit_count,
            &bit_offset,
            &decoded.data[output_index]
        );
        if (status != NPBC_STATUS_OK) {
            npbc_buffer_destroy(&decoded);
            return status;
        }
    }

    if (bit_offset != bit_count) {
        npbc_buffer_destroy(&decoded);
        return NPBC_STATUS_INVALID_FORMAT;
    }

    *output = decoded;
    return NPBC_STATUS_OK;
}
