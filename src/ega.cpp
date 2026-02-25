#include "ega.h"

/**
 * ega_align_up takes a size and alignment and returns the lowest multiple of
 * alignment >= size.
 */
static size_t ega_align_up(size_t v, size_t align) {
    EGA_ASSERT(align != 0);
    EGA_ASSERT((align & (align - 1)) == 0); // ensure align = 1, 2, 4, 8, 16...
    return (v + (align - 1)) & ~(align - 1);
}

/**
 * ega_arena_init initialises an arena backed by storage with a maximum size of
 * cap bytes.
 */
void ega_arena_init(ega_arena_t *a, void *storage, size_t cap) {
    a->base = (uint8_t *)storage;
    a->cap  = cap;
    a->off  = 0;
}

/**
 * ega_arena_alloc allocates space within the arena of size with the given
 * alignment.  Asserts that new allocation does not exceed arena size.
 */
void *ega_arena_alloc(ega_arena_t *a, size_t size, size_t align) {
    size_t off = ega_align_up(a->off, align);
    EGA_ASSERT(off <= a->cap);
    EGA_ASSERT(size <= (a->cap - off));
    void *p = a->base + off;
    a->off  = off + size;
    return p;
}

size_t ega_arena_mark(ega_arena_t *a) {
    return a->off;
}

void ega_arena_reset(ega_arena_t *a, size_t mark) {
    EGA_ASSERT(mark <= a->off);
    a->off = mark;
}

ega_buffer_t *ega_buffer_alloc(ega_arena_t *a, uint16_t w, uint16_t h) {
    return ega_buffer_alloc_stride(a, w, h, w);
}

ega_buffer_t *ega_buffer_alloc_stride(ega_arena_t *a, uint16_t w, uint16_t h,
                                      uint16_t stride) {
    EGA_ASSERT(stride >= w);

    size_t bytes = (size_t)stride * (size_t)h;
    size_t total =
        sizeof(ega_buffer_t) + bytes - 1; // -1 due to the flexible array member

    ega_buffer_t *b =
        (ega_buffer_t *)ega_arena_alloc(a, total, EGA_ALIGNOF(ega_buffer_t));
    b->w      = w;
    b->h      = h;
    b->stride = stride;

    memset(b->data, 0, bytes);
    return b;
}

size_t ega_buffer_data_bytes(const ega_buffer_t *b) {
    return (size_t)b->stride * (size_t)b->h;
}

/**
 * ega_render_buffer takes a destination buffer of variable size and renders the
 * contents of src by sampling each pixel in dst.  pal can be provided to map
 * palette indexes between one another and should be a pointer to a 16 index
 * array.
 */
void ega_render_buffer(uint32_t *dst, const uint8_t *src, const int w,
                       const int h, const uint8_t *pal) {
    for (int y = 0; y < h; y++) {
        int src_y = y * EGA_SCREEN_HEIGHT / h;
        for (int x = 0; x < w; x++) {
            int     src_x   = x * EGA_SCREEN_WIDTH / w;
            uint8_t ega_idx = src[src_y * EGA_SCREEN_WIDTH + src_x];
            if (pal != 0) {
                ega_idx = pal[ega_idx];
            }
            dst[y * w + x] = color_to_u32(EGA_PALETTE[ega_idx & 0x0F]);
        }
    }
}

/**
 * ega_decode_4_plane takes an input buffer, and decodes 16 index
 * bitplanes.egacpp len should be the number of bytes per plane.  len * 8 bytes
 * will be written to dst.  offset can be > 0 in cases where the bitplanes are
 * not sequential in src.
 */
void ega_decode_4_plane(uint8_t *dst, const uint8_t *src, uint16_t len,
                        uint16_t offset) {
    uint16_t stride = len + offset;
    for (int i = 0; i < len; i++) {
        uint8_t b1 = *(src + i + stride * 0);
        uint8_t b2 = *(src + i + stride * 1);
        uint8_t b3 = *(src + i + stride * 2);
        uint8_t b4 = *(src + i + stride * 3);
        for (int m = 0; m < 8; m++) {
            uint8_t mask = (uint8_t)(0x80 >> m);
            dst[8 * i + m] =
                (((b1 & mask) != 0) << 0) | (((b2 & mask) != 0) << 1) |
                (((b3 & mask) != 0) << 2) | (((b4 & mask) != 0) << 3);
        }
    }
}

void ega_decode_1_plane(uint8_t *dst, const uint8_t *src, uint16_t len) {
    for (int i = 0; i < len; i++) {
        for (int m = 0; m < 8; m++) {
            uint8_t mask   = (uint8_t)(0x80 >> m);
            dst[8 * i + m] = (uint8_t)((src[i] & mask) != 0);
        }
    }
}
