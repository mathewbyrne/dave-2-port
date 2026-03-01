#ifndef APP_H
#define APP_H

#include "ega.h"

#include <stdint.h>

enum {
    GAME_STATE_ASSET_CAP   = 1024 * 1024,
    GAME_STATE_SCRATCH_CAP = 128 * 1024,

    // Number of tiles in EGA_TILES
    GAME_TILES_WIDTH  = 13,
    GAME_TILES_HEIGHT = 66,
    GAME_TILES_COUNT  = GAME_TILES_WIDTH * GAME_TILES_HEIGHT,

    // TODO Ok these offsets are going to be brittle, depending on how someone is
    // able to uncompress the executable.  What we do probably know is that they
    // will be paragraph aligned since they will be at the start of a segment.
    EXEC_SPRITE_DATA_OFFSET = 0x18970,
    EXEC_SPRITE_DATA_STRIDE = 0x33B0,
    EXEC_SPRITE_COUNT       = 32,

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

typedef struct {
    uint8_t toggle_pause;
    uint8_t next_scene;
} game_input_t;

typedef enum {
    GAME_SCENE_LOADING,
    GAME_SCENE_TITLE,
    GAME_SCENE_LEVEL,
} game_scene_t;

typedef enum {
    GAME_MENU_HIDDEN,
    GAME_MENU_OPENING,
    GAME_MENU_OPEN,
} game_menu_state_t;

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

    game_scene_t      scene;
    game_menu_state_t menu_state;
    uint16_t          menu_anim_ticks;
    uint16_t          loading_step;
    uint16_t          loading_step_count;

    ega_buffer_t *exec_sprites[EXEC_SPRITE_COUNT];
    ega_buffer_t *glyphs[256];
    ega_buffer_t *tiles[GAME_TILES_COUNT];

    uint16_t level_w;
    uint16_t level_h;
    uint16_t level_tiles[64][64]; // TODO set sensible max and validate at loading
} game_state_t;

void game_init(game_state_t *state);
void game_tick(game_state_t *state, const game_input_t *input, float dt_seconds,
               uint32_t *out_pixels, int out_width, int out_height);

#endif
