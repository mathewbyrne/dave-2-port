#ifndef APP_H
#define APP_H

#include "ega.h"
#include "entities.h"

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
    GAME_ENTITY_CAP   = 164,

    // TODO Ok these offsets are going to be brittle, depending on how someone is
    // able to uncompress the executable.  What we do probably know is that they
    // will be paragraph aligned since they will be at the start of a segment.
    EXEC_SPRITE_DATA_OFFSET = 0x18970,
    EXEC_SPRITE_DATA_STRIDE = 0x33B0,
    EXEC_SPRITE_COUNT       = 32,
    EXEC_SPR_DEF_OFFSET     = 0x125F0,
    EXEC_SPR_DEF_RECORD_SIZE = 0x20,
    EXEC_SPR_DEF_PHASE_COUNT = 4,
    EXEC_SPR_DEF_GROUP_COUNT = 199,
    EXEC_SPR_DEF_RECORD_COUNT = EXEC_SPR_DEF_PHASE_COUNT * EXEC_SPR_DEF_GROUP_COUNT,

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
    LEVEL_TAG_HUNCH          = 0x0002,
    LEVEL_TAG_SLIME          = 0x0003,
    LEVEL_TAG_UNKNOWN_0x0004 = 0x0004,
    LEVEL_TAG_UNKNOWN_0x0005 = 0x0005,
    LEVEL_TAG_SPIDER         = 0x0006,
    LEVEL_TAG_UNKNOWN_0x0007 = 0x0007,
    LEVEL_TAG_UNKNOWN_0x0009 = 0x0009,
    LEVEL_TAG_UNKNOWN_0x000B = 0x000B,
    LEVEL_TAG_UNKNOWN_0x000C = 0x000C,
    LEVEL_TAG_UNKNOWN_0x000D = 0x000D,
    LEVEL_TAG_TREASURE1      = 0x000E,
    LEVEL_TAG_TREASURE2      = 0x000F,
    LEVEL_TAG_TREASURE3      = 0x0010,
    LEVEL_TAG_TREASURE4      = 0x0011,
    LEVEL_TAG_TREASURE5      = 0x0012,
    LEVEL_TAG_1UP            = 0x0013,
    LEVEL_TAG_PLAYER         = 0x00FF,
    LEVEL_TAG_UNKNOWN_0x0505 = 0x0505,
    LEVEL_TAG_UNKNOWN_0x0506 = 0x0506,
    LEVEL_TAG_TELEPORT_A1    = 0x0633,
    LEVEL_TAG_UNKNOWN_0x072A = 0x072A,
    LEVEL_TAG_TELEPORT_A2    = 0x0A06,
    LEVEL_TAG_UNKNOWN_0x0B15 = 0x0B15,
    LEVEL_TAG_UNKNOWN_0x0D42 = 0x0D42,
    LEVEL_TAG_TELEPORT_D1    = 0x1620,
    LEVEL_TAG_UNKNOWN_0x1919 = 0x1919,
    LEVEL_TAG_UNKNOWN_0x213E = 0x213E,
    LEVEL_TAG_UNKNOWN_0x2330 = 0x2330,
    LEVEL_TAG_TELEPORT_D2    = 0x240E,
    LEVEL_TAG_TELEPORT_B1    = 0x241C,
    LEVEL_TAG_UNKNOWN_0x2736 = 0x2736,
    LEVEL_TAG_TELEPORT_C1    = 0x2A0C,
    LEVEL_TAG_UNKNOWN_0x2B36 = 0x2B36,
    LEVEL_TAG_TELEPORT_E1    = 0x3007,
    LEVEL_TAG_TELEPORT_C2    = 0x301C,
    LEVEL_TAG_UNKNOWN_0x431A = 0x431A,
    LEVEL_TAG_TELEPORT_B2    = 0x4615,
    LEVEL_TAG_TELEPORT_E2    = 0x461C,
    LEVEL_TAG_UNKNOWN_0x4810 = 0x4810,
    LEVEL_TAG_UNKNOWN_0x492A = 0x492A,
    LEVEL_TAG_UNKNOWN_0x4B3A = 0x4B3A,
    LEVEL_TAG_DOOR_EXIT      = 0x8000,
    LEVEL_TAG_DOOR_TREASURE1 = 0x8001,
    LEVEL_TAG_DOOR_TREASURE2 = 0x8002,
    LEVEL_TAG_DOOR_TREASURE3 = 0x8003,
    LEVEL_TAG_DOOR_TREASURE4 = 0x8004,
    LEVEL_TAG_DOOR_TREASURE5 = 0x8005,
} level_tag_t;

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
} exec_sprite_def_record_t;

typedef struct {
    char                     name[13];
    exec_sprite_def_record_t phase[EXEC_SPR_DEF_PHASE_COUNT];
} exec_sprite_def_group_t;

typedef struct {
    ega_buffer_t *image;
    ega_buffer_t *mask;
    uint16_t      w;
    uint16_t      h;
} sprite_t;

typedef struct {
    // memory
    uint8_t  asset_mem[GAME_STATE_ASSET_CAP];
    uint8_t  scratch_mem[GAME_STATE_SCRATCH_CAP];
    entity_t entities_mem[GAME_ENTITY_CAP];

    ega_arena_t asset_arena;
    ega_arena_t scratch_arena;

    entities_arena_t entities;

    ega_buffer_t *buffer;

    ega_buffer_t *title_1;
    ega_buffer_t *title_2;

    ega_buffer_t *exec_sprites[EXEC_SPRITE_COUNT];
    exec_sprite_def_group_t exec_sprite_defs[EXEC_SPR_DEF_GROUP_COUNT];
    sprite_t      sprites[EXEC_SPR_DEF_GROUP_COUNT];
    ega_buffer_t *glyphs[256];
    ega_buffer_t *tiles[GAME_TILES_COUNT];

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
