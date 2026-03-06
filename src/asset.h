#ifndef ASSET_H
#define ASSET_H

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

#include "ega.h"

// TODO get rid of this it's not giving us anything over file_t
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

typedef struct {
    char          name[13];
    ega_buffer_t *image;
    ega_buffer_t *mask;
    uint16_t      w;
    uint16_t      h;
} sprite_t;

typedef struct {
    uint16_t      id;
    ega_buffer_t *gfx;
    bool          solid_n;
    bool          solid_e;
    bool          solid_w;
    bool          solid_s;
} tile_t;

enum {
    ASSET_LEVEL_MAX_W = 128,
    ASSET_LEVEL_MAX_H = 128,

    ASSET_TITLE_SCREENS = 3, // 2 title scrolling screens + loading screen

    ASSET_UI_SPRITE_COUNT = 32,

    ASSET_TILES_WIDTH  = 13,
    ASSET_TILES_HEIGHT = 66,
    ASSET_TILES_COUNT  = ASSET_TILES_WIDTH * ASSET_TILES_HEIGHT,

    ASSET_SPRITE_COUNT = 199,
};

typedef enum {
    LEVEL_TAG_ZOMBIE         = 0x0001,
    LEVEL_TAG_HUNCH          = 0x0002,
    LEVEL_TAG_SLIME          = 0x0003,
    LEVEL_TAG_UNKNOWN_0x0004 = 0x0004,
    LEVEL_TAG_UNKNOWN_0x0005 = 0x0005,
    LEVEL_TAG_SPIDER         = 0x0006,
    LEVEL_TAG_UNKNOWN_0x0007 = 0x0007,
    LEVEL_TAG_UNKNOWN_0x0009 = 0x0009,
    LEVEL_TAG_UNKNOWN_0x000B = 0x000B,
    LEVEL_TAG_UNKNOWN_0x000C = 0x000C,
    LEVEL_TAG_UNKNOWN_0x000D = 0x000D,
    LEVEL_TAG_TREASURE1      = 0x000E,
    LEVEL_TAG_TREASURE2      = 0x000F,
    LEVEL_TAG_TREASURE3      = 0x0010,
    LEVEL_TAG_TREASURE4      = 0x0011,
    LEVEL_TAG_TREASURE5      = 0x0012,
    LEVEL_TAG_1UP            = 0x0013,
    LEVEL_TAG_PLAYER         = 0x00FF,
    LEVEL_TAG_UNKNOWN_0x0505 = 0x0505,
    LEVEL_TAG_UNKNOWN_0x0506 = 0x0506,
    LEVEL_TAG_TELEPORT_A1    = 0x0633,
    LEVEL_TAG_UNKNOWN_0x072A = 0x072A,
    LEVEL_TAG_TELEPORT_A2    = 0x0A06,
    LEVEL_TAG_UNKNOWN_0x0B15 = 0x0B15,
    LEVEL_TAG_UNKNOWN_0x0D42 = 0x0D42,
    LEVEL_TAG_TELEPORT_D1    = 0x1620,
    LEVEL_TAG_UNKNOWN_0x1919 = 0x1919,
    LEVEL_TAG_UNKNOWN_0x213E = 0x213E,
    LEVEL_TAG_UNKNOWN_0x2330 = 0x2330,
    LEVEL_TAG_TELEPORT_D2    = 0x240E,
    LEVEL_TAG_TELEPORT_B1    = 0x241C,
    LEVEL_TAG_UNKNOWN_0x2736 = 0x2736,
    LEVEL_TAG_TELEPORT_C1    = 0x2A0C,
    LEVEL_TAG_UNKNOWN_0x2B36 = 0x2B36,
    LEVEL_TAG_TELEPORT_E1    = 0x3007,
    LEVEL_TAG_TELEPORT_C2    = 0x301C,
    LEVEL_TAG_UNKNOWN_0x431A = 0x431A,
    LEVEL_TAG_TELEPORT_B2    = 0x4615,
    LEVEL_TAG_TELEPORT_E2    = 0x461C,
    LEVEL_TAG_UNKNOWN_0x4810 = 0x4810,
    LEVEL_TAG_UNKNOWN_0x492A = 0x492A,
    LEVEL_TAG_UNKNOWN_0x4B3A = 0x4B3A,
    LEVEL_TAG_DOOR_EXIT      = 0x8000,
    LEVEL_TAG_DOOR_TREASURE1 = 0x8001,
    LEVEL_TAG_DOOR_TREASURE2 = 0x8002,
    LEVEL_TAG_DOOR_TREASURE3 = 0x8003,
    LEVEL_TAG_DOOR_TREASURE4 = 0x8004,
    LEVEL_TAG_DOOR_TREASURE5 = 0x8005,
} level_tag_t;

typedef struct {
    ega_buffer_t *titles[ASSET_TITLE_SCREENS];
    ega_buffer_t *ui_sprites[ASSET_UI_SPRITE_COUNT];
    ega_buffer_t *glyphs[256];
    tile_t        tiles[ASSET_TILES_COUNT];
    sprite_t      sprites[ASSET_SPRITE_COUNT];
} asset_shared_t;

typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t tiles[ASSET_LEVEL_MAX_W][ASSET_LEVEL_MAX_H];
    uint16_t tags[ASSET_LEVEL_MAX_W][ASSET_LEVEL_MAX_H];
} asset_level_t;

void asset_load_shared(asset_shared_t *shared, const char *basepath, ega_arena_t *a);

void asset_load_level(asset_level_t *level, const char *filename, const char *basepath);

#endif
