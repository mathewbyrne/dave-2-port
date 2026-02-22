#ifndef APP_H
#define APP_H

#include <stdint.h>

enum {
    GAME_SCREEN_WIDTH = 320,
    GAME_SCREEN_HEIGHT = 200,
    GAME_SCREEN_PIXELS = GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT,
};

typedef struct {
    int width;
    int height;

    uint8_t buffer[GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT];

    uint8_t title_pixels[2][GAME_SCREEN_PIXELS];

    uint16_t screen_offset_x;
    uint16_t t;
    float tick_accumulator;
    uint32_t tick;
} game_state_t;

void game_init(game_state_t *state, int width, int height);
void game_tick(game_state_t *state, float dt_seconds, uint32_t *out_pixels, int out_width, int out_height);

#endif
