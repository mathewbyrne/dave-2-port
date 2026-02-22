#ifndef EGA_H
#define EGA_H

#include <stdint.h>

enum {
    EGA_SCREEN_WIDTH = 320,
    EGA_SCREEN_HEIGHT = 200
};

void ega_decode_4_plane(uint8_t *dst, const uint8_t *src, uint16_t len, uint16_t offset);
void ega_decode_1_plane(uint8_t *dst, const uint8_t *src, uint16_t len);

#endif

