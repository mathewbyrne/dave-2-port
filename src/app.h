#ifndef APP_H
#define APP_H

#include "ega.h"

#include <stdint.h>

enum {
    GAME_STATE_ASSET_CAP   = 1024 * 1024,
    GAME_STATE_SCRATCH_CAP = 128 * 1024,
};

typedef struct {
    // memory
    uint8_t asset_mem[GAME_STATE_ASSET_CAP];
    uint8_t scratch_mem[GAME_STATE_SCRATCH_CAP];

    ega_arena_t asset_arena;
    ega_arena_t scratch_arena;

    ega_buffer_t *buffer;

    ega_buffer_t *title_1;
    ega_buffer_t *title_2;

    uint16_t width, height;

    uint16_t screen_offset_x;
    uint16_t t;
    float    tick_accumulator;
    uint32_t tick;
} game_state_t;

void game_init(game_state_t *state, int width, int height);
void game_tick(game_state_t *state, float dt_seconds, uint32_t *out_pixels,
               int out_width, int out_height);

#endif
