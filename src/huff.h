#ifndef HUFF_H
#define HUFF_H

/**
 * utilities for working with huffman encoded binary streams
 *
 * Byte streams should be encoded as follows:
 *
 * 0x0 4 bytes reading HUFF
 * 0x4 4 byte unsigned length of decoded stream
 * 0x8 256 byte huffman table
 *  TODO: describe the format of this table
 * 0x108..end byte array of encoded data
 */

#include <stddef.h>
#include <stdint.h>

struct huff_src {
    const uint8_t *ptr;
    size_t len;
};

typedef enum {
    HUFF_OK = 0,
    HUFF_ERR_BAD_MAGIC,
    HUFF_ERR_TRUNCATED,
    HUFF_ERR_NO_HEADER,
    HUFF_ERR_NODE_INDEX,
    HUFF_ERR_SRC_UNDERFLOW,
} huff_err;

#define HUFF_NODE_LIST_LEN 255
#define HUFF_NODE_LIST_ROOT 254
#define HUFF_NODE_LIST_SIZE 0x3FC

enum {
    HUFF_OFFSET_MAGIC     = 0x0,
    HUFF_OFFSET_LEN       = 0x4,
    HUFF_OFFSET_NODE_LIST = 0x8,
    HUFF_OFFSET_STREAM    = 0x8 + HUFF_NODE_LIST_SIZE,
};

/**
 * huff_len writes the length in bytes of a the uncompressed stream to out.
 * Returns non-zero if the src is not encoded correctly.
 */
huff_err huff_len(const huff_src src, size_t *out);

/**
 * huff_decode decodes the data from src into dst.  Ensure when calling that
 * src is a full span in the format described above.   len should be the number
 * of bytes expected to be written.  An error will be returned if the number
 * of bytes does not match.
 */
huff_err huff_decode(const huff_src src, uint8_t *dst, size_t len);


#endif

