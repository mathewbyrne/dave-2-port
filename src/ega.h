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
ega_buffer_t *ega_buffer_alloc_stride(ega_arena_t *a, uint16_t w, uint16_t h,
                                      uint16_t stride);
size_t        ega_buffer_data_bytes(const ega_buffer_t *b);

static inline uint8_t *ega_buffer_px(ega_buffer_t *b, uint16_t x, uint16_t y) {
    return b->data + (size_t)y * (size_t)b->stride + (size_t)x;
}

static inline const uint8_t *ega_buffer_cpx(const ega_buffer_t *b, uint16_t x,
                                            uint16_t y) {
    return b->data + (size_t)y * (size_t)b->stride + (size_t)x;
}

void ega_render_buffer(uint32_t *dst, const uint8_t *src, const int w,
                       const int h, const uint8_t *pal);
void ega_decode_4_plane(uint8_t *dst, const uint8_t *src, uint16_t len,
                        uint16_t offset);
void ega_decode_1_plane(uint8_t *dst, const uint8_t *src, uint16_t len);

#endif
