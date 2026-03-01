#ifndef DECODE_H
#define DECODE_H

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
    size_t         len;
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
huff_err huff_len(size_t *out, const huff_src src);

/**
 * huff_decode decodes the data from src into dst.  Ensure when calling that
 * src is a full span in the format described above.   len should be the number
 * of bytes expected to be written.  An error will be returned if the number
 * of bytes does not match.
 */
huff_err huff_decode(uint8_t *dst, size_t len, const huff_src src);

typedef enum {
    RLEW_OK = 0,
    RLEW_ERR_NO_HEADER,
    RLEW_ERR_TRUNCATED,
    RLEW_ERR_ODD_LENGTH,
    RLEW_ERR_DST_LEN_MISMATCH,
    RLEW_ERR_DST_OVERFLOW,
    RLEW_ERR_SRC_REMAINS,
} rlew_err;

enum {
    RLEW_WORD_MARKER = 0xFEFE,
    RLEW_OFFSET_LEN  = 0x0,
    RLEW_OFFSET_DATA = 0x4,
};

/**
 * rlew_len writes the byte length of the decoded stream to out.
 *
 * Source format:
 * - 4-byte little-endian uncompressed length in bytes
 * - followed by RLEW-encoded 16-bit words
 */
rlew_err rlew_len(size_t *out, const uint8_t *src, size_t src_len);

/**
 * rlew_decode expands a RLEW encoded source into dst.
 *
 * Encoding format (word-based):
 * - Any 16-bit word other than 0xFEFE is copied literally.
 * - 0xFEFE introduces a run encoded as: marker, count_word, value_word.
 *   count_word copies of value_word are emitted.
 *
 * The source starts with a 4-byte little-endian decoded-size header. dst_len
 * must exactly match this header value.
 *
 * The decoder requires exact consumption of src_len bytes. If trailing source
 * bytes remain after dst_len output bytes are produced, RLEW_ERR_SRC_REMAINS is
 * returned.
 */
rlew_err rlew_decode(uint8_t *dst, size_t dst_len, const uint8_t *src, size_t src_len);

#endif
