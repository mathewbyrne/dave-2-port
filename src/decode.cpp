#include "decode.h"

#include <string.h>

huff_err huff_validate(const huff_src src) {
    if (src.len < HUFF_OFFSET_STREAM) {
        return HUFF_ERR_NO_HEADER;
    }
    if (memcmp("HUFF", (char *)src.ptr, 4) != 0) {
        return HUFF_ERR_BAD_MAGIC;
    }
    return HUFF_OK;
}

huff_err huff_len(size_t *out, const huff_src src) {
    huff_err err = huff_validate(src);
    if (err != HUFF_OK) {
        return err;
    }

    long decoded_len = *(long *)(src.ptr + HUFF_OFFSET_LEN);
    *out             = (size_t)decoded_len;
    return HUFF_OK;
}

typedef struct {
    uint16_t bit0;
    uint16_t bit1;
} huff_node;

static huff_node  huff_node_list[HUFF_NODE_LIST_LEN + 1];
static huff_node *huff_node_root = huff_node_list + HUFF_NODE_LIST_ROOT;

huff_err huff_decode(uint8_t *dst, size_t len, const huff_src src) {
    huff_err err = huff_validate(src);
    if (err != HUFF_OK) {
        return err;
    }

    memcpy(huff_node_list, src.ptr + HUFF_OFFSET_NODE_LIST, HUFF_NODE_LIST_SIZE);

    const uint8_t *stream_ptr = src.ptr + HUFF_OFFSET_STREAM;
    huff_node     *node_ptr   = huff_node_root;
    size_t         dst_i      = 0;

    for (size_t i = 0; i < src.len; i++) {
        uint8_t data = stream_ptr[i];
        uint8_t mask = 1;
        for (int b = 0; b < 8; b++) {
            uint16_t next = (data & mask) ? node_ptr->bit1 : node_ptr->bit0;
            if (next < 0x100) {
                dst[dst_i++] = (uint8_t)next;
                if (dst_i >= len) {
                    return HUFF_OK;
                }
                node_ptr = huff_node_root;
            } else {
                uint16_t next_i = next - 0x100;
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

static uint16_t load_le16(const uint8_t *ptr) {
    return (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
}

static uint32_t load_le32(const uint8_t *ptr) {
    return (uint32_t)ptr[0] | ((uint32_t)ptr[1] << 8) | ((uint32_t)ptr[2] << 16) |
           ((uint32_t)ptr[3] << 24);
}

rlew_err rlew_len(size_t *out, const uint8_t *src, size_t src_len) {
    if (src_len < RLEW_OFFSET_DATA) {
        return RLEW_ERR_NO_HEADER;
    }

    size_t decoded_len = (size_t)load_le32(src + RLEW_OFFSET_LEN);
    if ((decoded_len & 1) != 0) {
        return RLEW_ERR_ODD_LENGTH;
    }

    *out = decoded_len;
    return RLEW_OK;
}

rlew_err rlew_decode(uint8_t *dst, size_t dst_len, const uint8_t *src, size_t src_len) {
    size_t   decoded_len = 0;
    rlew_err err         = rlew_len(&decoded_len, src, src_len);
    if (err != RLEW_OK) {
        return err;
    }
    if (decoded_len != dst_len) {
        return RLEW_ERR_DST_LEN_MISMATCH;
    }

    size_t si = RLEW_OFFSET_DATA;
    size_t di = 0;

    while (di < dst_len) {
        if ((si + 1) >= src_len) {
            return RLEW_ERR_TRUNCATED;
        }

        uint16_t word = load_le16(src + si);
        si += 2;

        if (word != RLEW_WORD_MARKER) {
            dst[di++] = (uint8_t)(word & 0x00FF);
            dst[di++] = (uint8_t)((word >> 8) & 0x00FF);
            continue;
        }

        if ((si + 3) >= src_len) {
            return RLEW_ERR_TRUNCATED;
        }

        uint16_t count     = load_le16(src + si);
        uint16_t value     = load_le16(src + si + 2);
        size_t   run_bytes = (size_t)count * 2;
        si += 4;

        if (run_bytes > (dst_len - di)) {
            return RLEW_ERR_DST_OVERFLOW;
        }

        for (uint16_t i = 0; i < count; i++) {
            dst[di++] = (uint8_t)(value & 0x00FF);
            dst[di++] = (uint8_t)((value >> 8) & 0x00FF);
        }
    }

    if (si != src_len) {
        return RLEW_ERR_SRC_REMAINS;
    }

    return RLEW_OK;
}
