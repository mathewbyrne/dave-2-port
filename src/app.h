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
    GAME_LEVEL_MAX_W  = 128,
    GAME_LEVEL_MAX_H  = 128,

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
    uint8_t move_left;
    uint8_t move_right;
    uint8_t move_up;
    uint8_t move_down;
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

typedef enum {
    LEVEL_TAG_ZOMBIE         = 0x0001,
    LEVEL_TAG_HUNCH_0x0002   = 0x0002,
    LEVEL_TAG_UNKNOWN_0x0003 = 0x0003,
    LEVEL_TAG_UNKNOWN_0x0004 = 0x0004,
    LEVEL_TAG_UNKNOWN_0x0005 = 0x0005,
    LEVEL_TAG_UNKNOWN_0x0006 = 0x0006,
    LEVEL_TAG_UNKNOWN_0x0007 = 0x0007,
    LEVEL_TAG_UNKNOWN_0x0009 = 0x0009,
    LEVEL_TAG_UNKNOWN_0x000B = 0x000B,
    LEVEL_TAG_UNKNOWN_0x000C = 0x000C,
    LEVEL_TAG_UNKNOWN_0x000D = 0x000D,
    LEVEL_TAG_UNKNOWN_0x000E = 0x000E,
    LEVEL_TAG_UNKNOWN_0x000F = 0x000F,
    LEVEL_TAG_UNKNOWN_0x0010 = 0x0010,
    LEVEL_TAG_UNKNOWN_0x0011 = 0x0011,
    LEVEL_TAG_UNKNOWN_0x0012 = 0x0012,
    LEVEL_TAG_UNKNOWN_0x0013 = 0x0013,
    LEVEL_TAG_PLAYER         = 0x00FF,
    LEVEL_TAG_UNKNOWN_0x0505 = 0x0505,
    LEVEL_TAG_UNKNOWN_0x0506 = 0x0506,
    LEVEL_TAG_TELEPORT1      = 0x0633,
    LEVEL_TAG_UNKNOWN_0x072A = 0x072A,
    LEVEL_TAG_UNKNOWN_0x0A06 = 0x0A06,
    LEVEL_TAG_UNKNOWN_0x0B15 = 0x0B15,
    LEVEL_TAG_UNKNOWN_0x0D42 = 0x0D42,
    LEVEL_TAG_UNKNOWN_0x1620 = 0x1620,
    LEVEL_TAG_UNKNOWN_0x1919 = 0x1919,
    LEVEL_TAG_UNKNOWN_0x213E = 0x213E,
    LEVEL_TAG_UNKNOWN_0x2330 = 0x2330,
    LEVEL_TAG_UNKNOWN_0x240E = 0x240E,
    LEVEL_TAG_UNKNOWN_0x241C = 0x241C,
    LEVEL_TAG_UNKNOWN_0x2736 = 0x2736,
    LEVEL_TAG_UNKNOWN_0x2A0C = 0x2A0C,
    LEVEL_TAG_UNKNOWN_0x2B36 = 0x2B36,
    LEVEL_TAG_UNKNOWN_0x3007 = 0x3007,
    LEVEL_TAG_UNKNOWN_0x301C = 0x301C,
    LEVEL_TAG_UNKNOWN_0x431A = 0x431A,
    LEVEL_TAG_UNKNOWN_0x4615 = 0x4615,
    LEVEL_TAG_UNKNOWN_0x461C = 0x461C,
    LEVEL_TAG_UNKNOWN_0x4810 = 0x4810,
    LEVEL_TAG_UNKNOWN_0x492A = 0x492A,
    LEVEL_TAG_UNKNOWN_0x4B3A = 0x4B3A,
    LEVEL_TAG_DOOR_EXIT      = 0x8000,
    LEVEL_TAG_DOOR_TREASURE1 = 0x8001,
    LEVEL_TAG_UNKNOWN_0x8002 = 0x8002,
    LEVEL_TAG_UNKNOWN_0x8003 = 0x8003,
    LEVEL_TAG_UNKNOWN_0x8004 = 0x8004,
    LEVEL_TAG_UNKNOWN_0x8005 = 0x8005,
} level_tag_t;

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
    uint16_t level_tiles[GAME_LEVEL_MAX_W][GAME_LEVEL_MAX_H];
    uint16_t level_plane2[GAME_LEVEL_MAX_W][GAME_LEVEL_MAX_H];
    uint16_t level_camera_x;
    uint16_t level_camera_y;
} game_state_t;

void game_init(game_state_t *state);
void game_tick(game_state_t *state, const game_input_t *input, float dt_seconds,
               uint32_t *out_pixels, int out_width, int out_height);

#endif
