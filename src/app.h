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
    uint8_t move_left;
    uint8_t move_right;
    uint8_t move_up;
    uint8_t move_down;
} game_direction_state_t;

typedef enum {
    GAME_DIRECTION_NONE = 0,
    GAME_DIRECTION_N,
    GAME_DIRECTION_NE,
    GAME_DIRECTION_E,
    GAME_DIRECTION_SE,
    GAME_DIRECTION_S,
    GAME_DIRECTION_SW,
    GAME_DIRECTION_W,
    GAME_DIRECTION_NW,
} game_direction_t;

typedef struct {
    uint8_t is_down;
    uint8_t pressed_this_frame;
} game_button_t;

typedef struct {
    game_direction_state_t direction_keys;
    game_direction_t       direction;

    game_button_t action_1;
    game_button_t action_2;

    uint8_t menu_help;
    uint8_t menu_sound_toggle;
    uint8_t menu_keyboard_config;
    uint8_t menu_reset_game;
    uint8_t menu_quit_game;
    uint8_t menu_status;
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
