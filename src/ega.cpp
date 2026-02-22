#include "ega.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color24_t;

static const color24_t EGA_PALETTE[16] = {
    { 0x00, 0x00, 0x00 }, // 00 Black
    { 0x00, 0x00, 0xAA }, // 01 Blue
    { 0x00, 0xAA, 0x00 }, // 02 Green
    { 0x00, 0xAA, 0xAA }, // 03 Cyan
    { 0xAA, 0x00, 0x00 }, // 04 Red
    { 0xAA, 0x00, 0xAA }, // 05 Magenta
    { 0xAA, 0x55, 0x00 }, // 06 Brown
    { 0xAA, 0xAA, 0xAA }, // 07 Light Gray
    { 0x55, 0x55, 0x55 }, // 08 Dark Gray
    { 0x55, 0x55, 0xFF }, // 09 Bright Blue
    { 0x55, 0xFF, 0x55 }, // 10 Bright Green
    { 0x55, 0xFF, 0xFF }, // 11 Bright Cyan
    { 0xFF, 0x55, 0x55 }, // 12 Bright Red
    { 0xFF, 0x55, 0xFF }, // 13 Bright Magenta
    { 0xFF, 0xFF, 0x55 }, // 14 Yellow
    { 0xFF, 0xFF, 0xFF }  // 15 White
};

static uint32_t color_to_u32(color24_t c) {
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/**
 * ega_render_buffer takes a destination buffer of variable size and renders the
 * contents of src by sampling each pixel in dst.  pal can be provided to map
 * palette indexes between one another and should be a pointer to a 16 index 
 * array.
 */
void ega_render_buffer(uint32_t *dst, const uint8_t *src, const int w, const int h, const uint8_t *pal) {
    for (int y = 0; y < h; y++) {
        int src_y = y * EGA_SCREEN_HEIGHT / h;
        for (int x = 0; x < w; x++) {
            int src_x = x * EGA_SCREEN_WIDTH / w;
            uint8_t ega_idx = src[src_y * EGA_SCREEN_WIDTH + src_x];
            if (pal != 0) {
                ega_idx = pal[ega_idx];
            }
            dst[y * w + x] = color_to_u32(EGA_PALETTE[ega_idx & 0x0F]);
        }
    }
}

/**
 * decode_ega_bitplanes takes an input buffer, and decodes 16 index bitplanes.
 * len should be the number of bytes per plane.  len * 8 bytes will be written
 * to dst.  offset can be > 0 in cases where the bitplanes are not sequential
 * in src.
 */
void ega_decode_4_plane(uint8_t *dst, const uint8_t *src, uint16_t len, uint16_t offset) {
    uint16_t stride = len + offset;
    for (int i = 0; i < len; i++) {
        uint8_t b1 = *(src + i + stride * 0);
        uint8_t b2 = *(src + i + stride * 1);
        uint8_t b3 = *(src + i + stride * 2);
        uint8_t b4 = *(src + i + stride * 3);
        for (int m = 0; m < 8; m++) {
            uint8_t mask = (uint8_t)(0x80 >> m);
            dst[8 * i + m] =
                (((b1 & mask) != 0) << 0) |
                (((b2 & mask) != 0) << 1) |
                (((b3 & mask) != 0) << 2) |
                (((b4 & mask) != 0) << 3);
        }
    }
}

void ega_decode_1_plane(uint8_t *dst, const uint8_t *src, uint16_t len) {
    for (int i = 0; i < len; i++) {
        for (int m = 0; m < 8; m++) {
            uint8_t mask = (uint8_t)(0x80 >> m);
            dst[8 * i + m] = (uint8_t)((src[i] & mask) != 0);
        }
    }
}

