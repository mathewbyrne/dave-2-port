#ifndef APP_H
#define APP_H

#include "asset.h"
#include "ega.h"
#include "entities.h"

#include <stdint.h>

enum {
    GAME_STATE_ASSET_CAP   = 1024 * 1024,
    GAME_STATE_SCRATCH_CAP = 128 * 1024,

    GAME_ENTITY_CAP = 164,
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
    GAME_SCENE_TITLE,
    GAME_SCENE_LEVEL,
} game_scene_t;

typedef enum {
    GAME_MENU_HIDDEN,
    GAME_MENU_OPENING,
    GAME_MENU_OPEN,
} game_menu_state_t;

// typedef struct {
//     uint16_t stride_bytes;
//     uint16_t height;
//     uint16_t unknown_0x04;
//     uint16_t unknown_0x06;
//     int16_t  bbox_dx0;
//     int16_t  bbox_dy0;
//     int16_t  bbox_dx1;
//     int16_t  bbox_dy1;
//     char     name[13];
//     int16_t  spawn_dx;
//     int16_t  spawn_dy;
// } exec_sprite_def_record_t;

// typedef struct {
//     char                     name[13];
//     exec_sprite_def_record_t phase[EXEC_SPR_DEF_PHASE_COUNT];
// } exec_sprite_def_group_t;

typedef struct {
    // memory
    uint8_t  asset_mem[GAME_STATE_ASSET_CAP];
    uint8_t  scratch_mem[GAME_STATE_SCRATCH_CAP];
    entity_t entities_mem[GAME_ENTITY_CAP];

    ega_arena_t asset_arena;
    ega_arena_t scratch_arena;

    entities_arena_t entities;

    ega_buffer_t *buffer;

    uint16_t width, height;

    uint16_t screen_offset_x;
    uint16_t t;
    float    tick_accumulator;
    uint32_t tick;

    game_scene_t      scene;
    game_menu_state_t menu_state;
    uint16_t          menu_anim_ticks;

    asset_shared_t assets;
    asset_level_t  level;

    uint16_t level_camera_x;
    uint16_t level_camera_y;

} game_state_t;

void game_init(game_state_t *state);
void game_tick(game_state_t *state, const game_input_t *input, float dt_seconds,
               uint32_t *out_pixels, int out_width, int out_height);

#endif
