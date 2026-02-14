#include "huff.h"

#include <string.h>

huff_err huff_validate(const huff_src src) {
    if (src.len < HUFF_OFFSET_STREAM) {
        return HUFF_ERR_NO_HEADER;
    }
    if (memcmp("HUFF", (char *) src.ptr, 4) != 0) {
        return HUFF_ERR_BAD_MAGIC;
    }
    return HUFF_OK;
}

huff_err huff_len(const huff_src src, size_t *out) {
    huff_err err = huff_validate(src);
    if (err != HUFF_OK) {
        return err;
    }

    // 4 bytes are used to store the offset, we need to widen that to sizeof(size_t)
    long _out = *(long *)(src.ptr + HUFF_OFFSET_LEN);
    *out = (size_t) _out;
    return HUFF_OK;
}

struct huff_node {
    uint16_t bit0;
    uint16_t bit1;
};

static huff_node huff_node_list[HUFF_NODE_LIST_LEN + 1];
static huff_node *huff_node_root = huff_node_list + HUFF_NODE_LIST_ROOT;

huff_err huff_decode(const huff_src src, uint8_t *dst, size_t len) {
    huff_err err = huff_validate(src);
    if (err != HUFF_OK) {
        return err;
    }

    memcpy(huff_node_list, src.ptr + HUFF_OFFSET_NODE_LIST, HUFF_NODE_LIST_SIZE);

    const uint8_t *stream_ptr = src.ptr + HUFF_OFFSET_STREAM;
    huff_node *node_ptr = huff_node_root;
    int dst_i = 0;

    for (int i = 0; i < src.len; i++) {
        uint8_t data = stream_ptr[i];
        uint8_t mask = 1;
        for (int b = 0; b < 8; b++) {
            uint16_t next = (data & mask) ? node_ptr->bit1 : node_ptr->bit0;
            if (next < 0x100) {
                dst[dst_i++] = (uint8_t) next;
                if (dst_i >= len) {
                    return HUFF_OK;
                }
                node_ptr = huff_node_root;
            } else {
                int next_i = next - 0x100;
                if (next_i >= HUFF_NODE_LIST_LEN) {
                    return HUFF_ERR_NODE_INDEX;
                }
                node_ptr = huff_node_list + next_i;
            }
            mask <<= 1;
        }
    }

    return HUFF_ERR_SRC_UNDERFLOW;
}

