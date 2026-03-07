#include "asset.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_le_u16(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static int16_t read_le_i16(const uint8_t *src) {
    return (int16_t)read_le_u16(src);
}

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

    uint32_t decoded_len = (uint32_t)src.ptr[HUFF_OFFSET_LEN + 0] |
                           ((uint32_t)src.ptr[HUFF_OFFSET_LEN + 1] << 8) |
                           ((uint32_t)src.ptr[HUFF_OFFSET_LEN + 2] << 16) |
                           ((uint32_t)src.ptr[HUFF_OFFSET_LEN + 3] << 24);
    *out                 = (size_t)decoded_len;
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

typedef struct {
    uint8_t *data;
    size_t   len;
} file_t;

static int load_file(const char *filename, const char *basepath, file_t *dst) {
    char  path[1024];
    FILE *fp = 0;

    if (basepath && basepath[0]) {
        int path_len = snprintf(path, sizeof(path), "%s/%s", basepath, filename);
        if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
            return 1;
        }
        filename = path;
    }

#if defined(_MSC_VER)
    if (fopen_s(&fp, filename, "rb") != 0) {
        return 1;
    }
#else
    fp = fopen(filename, "rb");
    if (!fp) {
        return 1;
    }
#endif

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }

    long file_len = ftell(fp);
    if (file_len < 0) {
        fclose(fp);
        return 1;
    }

    rewind(fp);

    dst->len  = (size_t)file_len;
    dst->data = (uint8_t *)malloc(file_len);
    if (!dst->data) {
        fclose(fp);
        return 1;
    }

    if (fread(dst->data, 1, dst->len, fp) != dst->len) {
        free(dst->data);
        dst->data = 0;
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

typedef enum {
    ENCODING_NONE = 0,
    ENCODING_RLEW = 1,
    ENCODING_HUFF = 2,
    ENCODING_LZ   = 3, // TODO eventually we will want to implement decoding LZEXE
} encoding_t;

static int load_file_decode(const char *filename, const char *basepath, encoding_t enc,
                            file_t *dst) {
    switch (enc) {
    case ENCODING_NONE:
    case ENCODING_LZ: // TODO allocate LZ buffer then call load_file, treat it as no encoding for
                      // the time being, since the user can extract their own executable.
        return load_file(filename, basepath, dst);

    case ENCODING_RLEW: {
        file_t f   = {};
        int    err = load_file(filename, basepath, &f);
        if (err != 0) {
            free(f.data);
            return err;
        }

        dst->len = 0;
        err      = rlew_len(&dst->len, f.data, f.len);
        if (err != 0) {
            return err;
        }

        dst->data = (uint8_t *)malloc(dst->len);
        err       = rlew_decode(dst->data, dst->len, f.data, f.len);
        free(f.data);
        return err;
    }

    case ENCODING_HUFF: {
        file_t f   = {};
        int    err = load_file(filename, basepath, &f);
        if (err != 0) {
            free(f.data);
            return err;
        }

        huff_src h = {.ptr = f.data, .len = f.len};

        dst->len = 0;
        err      = huff_len(&dst->len, h);
        if (err != 0) {
            return err;
        }

        dst->data = (uint8_t *)malloc(dst->len);
        err       = huff_decode(dst->data, dst->len, h);
        free(f.data);
        return err;
    }
    }

    return 1;
}

typedef struct {
    file_t exec;
    file_t tiles;
    file_t sprites[5];
    file_t titles[3];
} asset_raw_t;

typedef struct {
    uint16_t first_sprite_id;
    uint16_t sprite_count;
} sprite_file_info_t;

static const sprite_file_info_t g_sprite_files[] = {
    {0x00, 85}, {0x55, 36}, {0x79, 53}, {0xAE, 13}, {0xBB, 12},
};

static_assert((85 + 36 + 53 + 13 + 12) == ASSET_SPRITE_COUNT);

enum {
    TITLE_FILE_HEADER_BYTES = 8,
    TITLE_FILE_IMAGE_BYTES  = 4 * (EGA_SCREEN_WIDTH * EGA_SCREEN_HEIGHT / 8),
    TITLE_FILE_BYTES        = TITLE_FILE_HEADER_BYTES + TITLE_FILE_IMAGE_BYTES,
};

int decode_titles(asset_shared_t *shared, const asset_raw_t raw, ega_arena_t *a) {
    for (int i = 0; i < 3; i++) {
        assert(raw.titles[i].len == TITLE_FILE_BYTES);
        shared->titles[i] = ega_buffer_alloc(a, EGA_SCREEN_WIDTH, EGA_SCREEN_HEIGHT);

        ega_decode_4_plane(shared->titles[i]->data, raw.titles[i].data + TITLE_FILE_HEADER_BYTES,
                           EGA_SCREEN_WIDTH * EGA_SCREEN_HEIGHT / 8, 0);
    }
    return 0;
}

enum {
    ASSET_TILE_WIDTH  = 16,
    ASSET_TILE_HEIGHT = 16,

    // Tile metadata is not stored in the tile graphics file, but in the executable.
    EXEC_TILE_DATA_OFFSET = 0x285EA
};

int decode_tiles(asset_shared_t *shared, const asset_raw_t raw, ega_arena_t *a) {
    uint16_t stride = ASSET_TILE_WIDTH * ASSET_TILE_HEIGHT / 8;
    assert(raw.tiles.len == stride * 4 * ASSET_TILES_COUNT);

    // each tile metadata table is a series of bytes
    uint8_t *floor_tbl = raw.exec.data + EXEC_TILE_DATA_OFFSET + ASSET_TILES_COUNT * 0;
    uint8_t *ceil_tbl  = raw.exec.data + EXEC_TILE_DATA_OFFSET + ASSET_TILES_COUNT * 1;
    uint8_t *left_tbl  = raw.exec.data + EXEC_TILE_DATA_OFFSET + ASSET_TILES_COUNT * 2;
    uint8_t *right_tbl = raw.exec.data + EXEC_TILE_DATA_OFFSET + ASSET_TILES_COUNT * 3;

    for (uint16_t i = 0; (i + 1) * stride * 4 < raw.tiles.len; i += 1) {
        shared->tiles[i].id  = i;
        shared->tiles[i].gfx = ega_buffer_alloc(a, ASSET_TILE_WIDTH, ASSET_TILE_HEIGHT);
        ega_decode_4_plane(shared->tiles[i].gfx->data, raw.tiles.data + i * stride * 4, stride, 0);

        shared->tiles[i].solid_floor = floor_tbl[i];
        shared->tiles[i].solid_ceil  = ceil_tbl[i];
        shared->tiles[i].solid_left  = left_tbl[i];
        shared->tiles[i].solid_right = right_tbl[i];
    }

    return 0;
}

enum {
    // Format of the font storage within the executable:
    //
    // 0x000 uint16_t font height
    // 0x002 uint16_t[256] table of character offsets
    // 0x202 uint8[256] table of character widths in pixels
    // 0x302 font pixel data
    //
    // A glyph is stored as a bitplane of ((w + 7) >> 3) * h bytes, however each offset stores
    // 2x this.  The first block is some sort of EGA optimised draw mask, the second is the glyph
    // pixel data.  So the second block at each character offset stores our mask.
    EXEC_FONT_SEG_OFFSET     = 0x10740,
    EXEC_FONT_OFFSETS_OFFSET = EXEC_FONT_SEG_OFFSET + 2,
    EXEC_FONT_WIDTHS_OFFSET  = EXEC_FONT_SEG_OFFSET + 2 + (256 * 2),
    EXEC_FONT_PIXEL_OFFSET   = EXEC_FONT_SEG_OFFSET + 2 + (256 * 2) + 256,
};

int decode_glyphs(asset_shared_t *shared, const asset_raw_t raw, ega_arena_t *a) {
    uint16_t char_h = *(uint16_t *)(raw.exec.data + EXEC_FONT_SEG_OFFSET);
    for (int i = 0; i < 256; i++) {
        uint16_t char_offset = *(uint16_t *)(raw.exec.data + EXEC_FONT_OFFSETS_OFFSET + i * 2);
        // Some characters just don't have sprite mappings.  Let's leave these as NULL for the
        // time being, however maybe we can just have an empty sprite we assign for these?  A
        // buffer with w = 0, h = 0 should render ok via the blitter.
        if (char_offset == 0) {
            continue;
        }
        uint8_t  char_w        = *(uint8_t *)(raw.exec.data + EXEC_FONT_WIDTHS_OFFSET + i);
        uint16_t bytes_per_row = (char_w + 7) >> 3;
        uint16_t block_size    = bytes_per_row * char_h;

        shared->glyphs[i] = ega_buffer_alloc(a, char_w, char_h);
        ega_decode_1bpp(shared->glyphs[i]->data,
                        raw.exec.data + EXEC_FONT_SEG_OFFSET + char_offset + block_size, char_w,
                        char_h);
    }

    return 0;
}

enum {
    EXEC_SPR_DEF_OFFSET      = 0x125F0,
    EXEC_SPR_DEF_RECORD_SIZE = 0x20,
};

typedef struct {
    uint16_t stride_bytes;
    uint16_t height;
    uint16_t unknown_0x04;
    uint16_t unknown_0x06;
    int16_t  bbox_dx0;
    int16_t  bbox_dy0;
    int16_t  bbox_dx1;
    int16_t  bbox_dy1;
    char     name[13];
    int16_t  spawn_dx;
    int16_t  spawn_dy;
} spr_def;

int decode_sprites(asset_shared_t *shared, const asset_raw_t raw, ega_arena_t *a) {
    const size_t sprite_def_data_end =
        (size_t)EXEC_SPR_DEF_OFFSET + ((size_t)ASSET_SPRITE_COUNT * 4 * EXEC_SPR_DEF_RECORD_SIZE);
    assert(sprite_def_data_end <= raw.exec.len);

    struct {
        char    name[13];
        spr_def phase[4];
    } spr_defs[ASSET_SPRITE_COUNT];

    // First we need to collect the sprite definitions from the executable.  See the
    // dos-implementation docs for details on how these are stored.
    for (int i = 0; i < ASSET_SPRITE_COUNT; i++) {
        for (int j = 0; j < 4; j++) {
            size_t record_i   = (size_t)i * 4 + (size_t)j;
            size_t record_off = (size_t)EXEC_SPR_DEF_OFFSET + record_i * EXEC_SPR_DEF_RECORD_SIZE;

            spr_def *record      = &spr_defs[i].phase[j];
            record->stride_bytes = read_le_u16(raw.exec.data + record_off + 0x00);
            record->height       = read_le_u16(raw.exec.data + record_off + 0x02);
            record->unknown_0x04 = read_le_u16(raw.exec.data + record_off + 0x04);
            record->unknown_0x06 = read_le_u16(raw.exec.data + record_off + 0x06);
            record->bbox_dx0     = read_le_i16(raw.exec.data + record_off + 0x08);
            record->bbox_dy0     = read_le_i16(raw.exec.data + record_off + 0x0A);
            record->bbox_dx1     = read_le_i16(raw.exec.data + record_off + 0x0C);
            record->bbox_dy1     = read_le_i16(raw.exec.data + record_off + 0x0E);
            memcpy(record->name, raw.exec.data + record_off + 0x10, 12);
            record->name[12] = 0;
            record->spawn_dx = read_le_i16(raw.exec.data + record_off + 0x1C);
            record->spawn_dy = read_le_i16(raw.exec.data + record_off + 0x1E);

            if (j == 0) {
                memcpy(spr_defs[i].name, record->name, sizeof(spr_defs[i].name));
            }
        }
    }

    for (size_t f = 0; f < (sizeof(g_sprite_files) / sizeof(g_sprite_files[0])); f++) {
        const sprite_file_info_t *file_info   = &g_sprite_files[f];
        const file_t             *sprite_file = &raw.sprites[f];

        assert(sprite_file->len >= 2);

        uint16_t chunk_size = read_le_u16(sprite_file->data + 0x00);
        assert(((size_t)chunk_size * 5) + 2 <= sprite_file->len);

        const uint8_t *plane_data = sprite_file->data + 0x02;
        const uint8_t *mask_data  = sprite_file->data + 0x02 + ((size_t)chunk_size * 4);

        for (uint16_t i = 0; i < file_info->sprite_count; i++) {
            uint16_t       sprite_id = (uint16_t)(file_info->first_sprite_id + i);
            const spr_def *def       = &spr_defs[sprite_id].phase[0];
            sprite_t      *dst       = &shared->sprites[sprite_id];

            assert(sprite_id < ASSET_SPRITE_COUNT);

            memcpy(dst->name, spr_defs[sprite_id].name, sizeof(dst->name));

            if (def->stride_bytes == 0 || def->height == 0) {
                continue;
            }

            uint16_t w = (uint16_t)(def->stride_bytes * 8);
            uint16_t h = def->height;

            size_t   sprite_len   = (size_t)def->stride_bytes * (size_t)def->height;
            size_t   sprite_off   = ((size_t)def->unknown_0x06 * 16) + (size_t)def->unknown_0x04;
            uint16_t plane_offset = (uint16_t)(chunk_size - sprite_len);

            assert(sprite_len <= chunk_size);
            assert((sprite_off + sprite_len) <= chunk_size);

            dst->image = ega_buffer_alloc(a, w, h);
            dst->mask  = ega_buffer_alloc(a, w, h);
            dst->w     = w;
            dst->h     = h;

            ega_decode_4_plane(dst->image->data, plane_data + sprite_off, (uint16_t)sprite_len,
                               plane_offset);
            ega_decode_1bpp(dst->mask->data, mask_data + sprite_off, w, h);
        }
    }

    return 0;
}

// TODO Ok these offsets are going to be brittle, depending on how someone is
// able to uncompress the executable.  What we do probably know is that they
// will be paragraph aligned since they will be at the start of a segment.
enum {
    EXEC_SPRITE_COUNT       = 32,
    EXEC_SPRITE_DATA_OFFSET = 0x18970,
    EXEC_SPRITE_DATA_STRIDE = 0x33B0,
};

static_assert((EXEC_SPRITE_COUNT * 8) <= EXEC_SPRITE_DATA_STRIDE);

int decode_ui_sprites(asset_shared_t *shared, const asset_raw_t raw, ega_arena_t *a) {
    const size_t sprite_plane_span = (size_t)EXEC_SPRITE_DATA_STRIDE;
    const size_t sprite_data_end =
        (size_t)EXEC_SPRITE_DATA_OFFSET + (size_t)(EXEC_SPRITE_COUNT * 8) + (3 * sprite_plane_span);
    assert(sprite_data_end <= raw.exec.len);

    uint16_t offset = EXEC_SPRITE_DATA_STRIDE - 8;
    for (int i = 0; i < EXEC_SPRITE_COUNT; i++) {
        shared->ui_sprites[i] = ega_buffer_alloc(a, 8, 8);
        ega_decode_4_plane(shared->ui_sprites[i]->data,
                           raw.exec.data + EXEC_SPRITE_DATA_OFFSET + 8 * i, 8, offset);
    }

    return 0;
}

void asset_load_shared(asset_shared_t *shared, const char *basepath, ega_arena_t *a) {
    asset_raw_t data = {};

    assert(load_file_decode("DAVE.EXE", basepath, ENCODING_LZ, &data.exec) == 0);
    assert(load_file_decode("EGATILES.DD2", basepath, ENCODING_NONE, &data.tiles) == 0);
    assert(load_file_decode("S_DAVE.DD2", basepath, ENCODING_HUFF, &data.sprites[0]) == 0);
    assert(load_file_decode("S_CHUNK1.DD2", basepath, ENCODING_HUFF, &data.sprites[1]) == 0);
    assert(load_file_decode("S_CHUNK2.DD2", basepath, ENCODING_HUFF, &data.sprites[2]) == 0);
    assert(load_file_decode("S_FRANK.DD2", basepath, ENCODING_HUFF, &data.sprites[3]) == 0);
    assert(load_file_decode("S_MASTER.DD2", basepath, ENCODING_HUFF, &data.sprites[4]) == 0);
    assert(load_file_decode("TITLE1.DD2", basepath, ENCODING_NONE, &data.titles[0]) == 0);
    assert(load_file_decode("TITLE2.DD2", basepath, ENCODING_NONE, &data.titles[1]) == 0);
    assert(load_file_decode("PROGPIC.DD2", basepath, ENCODING_HUFF, &data.titles[2]) == 0);

    assert(decode_titles(shared, data, a) == 0);
    assert(decode_tiles(shared, data, a) == 0);
    assert(decode_glyphs(shared, data, a) == 0);
    assert(decode_sprites(shared, data, a) == 0);
    assert(decode_ui_sprites(shared, data, a) == 0);

    free(data.exec.data);
    free(data.tiles.data);
    free(data.sprites[0].data);
    free(data.sprites[1].data);
    free(data.sprites[2].data);
    free(data.sprites[3].data);
    free(data.sprites[4].data);
    free(data.titles[0].data);
    free(data.titles[1].data);
    free(data.titles[2].data);
}

void asset_load_level(asset_level_t *level, const char *filename, const char *basepath) {
    file_t f = {};
    assert(load_file(filename, basepath, &f) == 0);

    size_t decode_len = 0;
    assert(rlew_len(&decode_len, f.data, f.len) == 0);

    uint8_t *decode_data = (uint8_t *)malloc(decode_len);
    assert(rlew_decode(decode_data, decode_len, f.data, f.len) == 0);

    // TODO There is a decent amount of level information in the header that is unused.  This data
    // is pretty much the same between all level files, which might indicate that it's some default
    // values from the authoring tools and not of any real use in the game engine.
    uint16_t width        = *(uint16_t *)(decode_data + 0x00);
    uint16_t height       = *(uint16_t *)(decode_data + 0x02);
    uint16_t plane_count  = *(uint16_t *)(decode_data + 0x04);
    uint16_t plane_stride = *(uint16_t *)(decode_data + 0x0E);
    size_t   plane0_off   = 0x20;
    size_t   plane1_off   = plane0_off + plane_stride;

    assert(width <= ASSET_LEVEL_MAX_W);
    assert(height <= ASSET_LEVEL_MAX_H);
    assert(plane_count >= 2);
    assert((plane1_off + plane_stride) <= decode_len);

    level->w = width;
    level->h = height;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx         = (size_t)y * (size_t)width + (size_t)x; // word index
            level->tiles[x][y] = *(uint16_t *)(decode_data + plane0_off + idx * 2);
            level->tags[x][y]  = *(uint16_t *)(decode_data + plane1_off + idx * 2);
        }
    }

    free(f.data);
    free(decode_data);
}
