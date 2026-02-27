#ifndef EGA_H
#define EGA_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define EGA_ALIGNOF(T) alignof(T)

#ifndef EGA_ASSERT
#include <assert.h>
#define EGA_ASSERT(x) assert(x)
#endif

enum {
    EGA_SCREEN_WIDTH  = 320,
    EGA_SCREEN_HEIGHT = 200,
};

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color24_t;

static inline uint32_t color_to_u32(color24_t c) {
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

static const color24_t EGA_PALETTE[16] = {
    {0x00, 0x00, 0x00}, // 00 Black
    {0x00, 0x00, 0xAA}, // 01 Blue
    {0x00, 0xAA, 0x00}, // 02 Green
    {0x00, 0xAA, 0xAA}, // 03 Cyan
    {0xAA, 0x00, 0x00}, // 04 Red
    {0xAA, 0x00, 0xAA}, // 05 Magenta
    {0xAA, 0x55, 0x00}, // 06 Brown
    {0xAA, 0xAA, 0xAA}, // 07 Light Gray
    {0x55, 0x55, 0x55}, // 08 Dark Gray
    {0x55, 0x55, 0xFF}, // 09 Bright Blue
    {0x55, 0xFF, 0x55}, // 10 Bright Green
    {0x55, 0xFF, 0xFF}, // 11 Bright Cyan
    {0xFF, 0x55, 0x55}, // 12 Bright Red
    {0xFF, 0x55, 0xFF}, // 13 Bright Magenta
    {0xFF, 0xFF, 0x55}, // 14 Yellow
    {0xFF, 0xFF, 0xFF}  // 15 White
};

typedef struct ega_arena_t {
    uint8_t *base;
    size_t   cap;
    size_t   off;
} ega_arena_t;

void   ega_arena_init(ega_arena_t *a, void *mem, size_t cap);
void  *ega_arena_alloc(ega_arena_t *a, size_t size, size_t align);
size_t ega_arena_mark(ega_arena_t *a);
void   ega_arena_reset(ega_arena_t *a, size_t mark);
void   ega_arena_clear(ega_arena_t *a);

typedef struct ega_buffer_t {
    uint16_t w;
    uint16_t h;
    uint16_t stride;  // bytes per row
    uint8_t  data[1]; // variable-sized tail allocation
} ega_buffer_t;

ega_buffer_t *ega_buffer_alloc(ega_arena_t *a, uint16_t w,
                               uint16_t h); // stride == w
ega_buffer_t *ega_buffer_alloc_stride(ega_arena_t *a, uint16_t w, uint16_t h, uint16_t stride);
size_t        ega_buffer_data_bytes(const ega_buffer_t *b);

static inline uint8_t *ega_buffer_px(ega_buffer_t *b, uint16_t x, uint16_t y) {
    return b->data + (size_t)y * (size_t)b->stride + (size_t)x;
}

/**
 * ega_buffer_blit copies src pixels into dst.  Both source and destination coordinates and widths
 * are signed and can be outside either source or destination, which will result in no pixels
 * copied.
 */
void ega_buffer_blit(ega_buffer_t *dst, const ega_buffer_t *src, int dst_x, int dst_y, int src_x,
                     int src_y, int w, int h);
/**
 * ega_buffer_blit_masked has the same geometry semantics as ega_buffer_blit, but additionally takes
 * a mask buffer.  A 0 index in a mask indicates transparent, where non-zero equals opaque.  Mask
 * is sampled in source space, and should match src dims.
 */
void ega_buffer_blit_masked(ega_buffer_t *dst, const ega_buffer_t *src, const ega_buffer_t *mask,
                            int dst_x, int dst_y, int src_x, int src_y, int w, int h);

/**
 * ega_buffer_sprite has the same semantics as ega_buffer_blit_masked but always blits the full
 * src buffer from 0, 0, to w, h.
 */
void ega_buffer_blit_sprite(ega_buffer_t *dst, const ega_buffer_t *src, const ega_buffer_t *mask,
                            int dst_x, int dst_y);

/**
 * ega_buffer_blit_colour takes a mask buffer and writes into dst with colour into unmasked bits.
 */
void ega_buffer_blit_colour(ega_buffer_t *dst, const ega_buffer_t *mask, uint8_t colour, int dst_x,
                            int dst_y);

/**
 * ega_render_buffer takes a destination buffer of variable size and renders the contents of src by
 * sampling each pixel in dst.  pal can be provided to map palette indexes between one another and
 * should be a pointer to a 16 index array.
 */
void ega_render_buffer(uint32_t *dst, const uint8_t *src, const int w, const int h,
                       const uint8_t *pal);

/**
 * ega_decode_4_plane takes an input buffer, and decodes 16 index bitplanes.egacpp len should be the
 * number of bytes per plane.  len * 8 bytes will be written to dst.  offset can be > 0 in cases
 * where the bitplanes are not sequential in src.
 *
 * ega_decode_1_plane is the same but for a single bitplane, primarily used for transparency masks.
 */
void ega_decode_4_plane(uint8_t *dst, const uint8_t *src, uint16_t len, uint16_t offset);
void ega_decode_1_plane(uint8_t *dst, const uint8_t *src, uint16_t len);

#endif
