#ifndef ENTITY_H
#define ENTITY_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ENTITY_TYPE_FREE   = 0x00,
    ENTITY_TYPE_PLAYER = 0x01,
    ENTITY_TYPE_ZOMBIE = 0x02,
    // unknown = 0x03
    // unknown = 0x04
    // unknown = 0x05
    // unknown = 0x06
    // unknown = 0x07
    // unknown = 0x08
    ENTITY_TYPE_PICKUP = 0x09,
    // unknown = 0x0a
    // unknown = 0x0b
    // unknown = 0x0c
    // unknown = 0x0d
    // unknown = 0x0e
    // unknown = 0x0f
    // unknown = 0x10
    // unknown = 0x11
    ENTITY_TYPE_SCORE = 0x12,
} entity_type_t;

typedef struct game_state_t game_state_t;
typedef struct entity_t     entity_t;

typedef void (*entity_update_fn)(game_state_t *game, entity_t *e);
typedef void (*entity_collide_entity_fn)(game_state_t *game, entity_t *e, entity_t *other);
typedef void (*entity_collide_world_fn)(game_state_t *game, entity_t *e);

struct entity_state_t {
    uint16_t                 sprite_left_idx;
    uint16_t                 sprite_right_idx;
    uint16_t                 interactive;
    uint16_t                 tick_period;
    int16_t                  x_step;
    int16_t                  y_step;
    entity_update_fn         on_update;
    entity_collide_entity_fn on_collide_entity;
    entity_collide_world_fn  on_collide_world;
    const entity_state_t    *next_state;
};

typedef struct entity_t {
    entity_type_t   type; // 0 means free slot / destroyed
    uint8_t         active;
    const sprite_t *sprite;

    int16_t x;
    int16_t y;
    int8_t  x_dir;
    int8_t  y_dir;
    int16_t vx;
    int16_t vy;
    int16_t ax;
    int16_t ay;

    int16_t tile_x0;
    int16_t tile_y0;
    int16_t tile_x1;
    int16_t tile_y1;

    int16_t bbox_x0;
    int16_t bbox_y0;
    int16_t bbox_x1;
    int16_t bbox_y1;

    bool blocked_n;
    bool blocked_e;
    bool blocked_s;
    bool blocked_w;

    uint16_t tick_accum;

    const entity_state_t *state;
} entity_t;

typedef struct entity_arena_t {
    entity_t *items;
    uint16_t  cap;
    uint16_t  count;
    uint16_t  next_free_hint;
} entity_arena_t;

/**
 * entity_arena_init initialises an arena backed by caller-provided storage.
 * storage must point to at least entity_storage_bytes(cap) bytes.
 */
void entity_arena_init(entity_arena_t *arena, void *storage, uint16_t cap);

/**
 * entity_arena_clear clears all entities and resets allocation state.
 */
void entity_arena_clear(entity_arena_t *arena);

/**
 * entity_alloc returns a free entity slot (where type == 0), clears it,
 * assigns type and marks active = 1. Returns 0 if no free slot exists.
 */
entity_t *entity_alloc(entity_arena_t *arena, uint16_t type);

/**
 * entity_destroy clears active and sets type = 0 so the slot can be reused.
 */
void entity_destroy(entity_arena_t *arena, entity_t *entity);

/**
 * entity_arena_data_bytes returns bytes used by the contiguous entity array.
 */
size_t entity_arena_data_bytes(const entity_arena_t *arena);
size_t entity_storage_bytes(uint16_t cap);

static inline entity_t *entity_begin(entity_arena_t *arena) {
    return arena->items;
}

static inline entity_t *entity_end(entity_arena_t *arena) {
    return arena->items + arena->cap;
}

static inline const entity_t *entity_begin_const(const entity_arena_t *arena) {
    return arena->items;
}

static inline const entity_t *entity_end_const(const entity_arena_t *arena) {
    return arena->items + arena->cap;
}

void entity_set_state(game_state_t *state, entity_t *e, const entity_state_t *s);

void player_idle_update(game_state_t *state, entity_t *e);                          // 3b98
void player_idle_collide_entity(game_state_t *state, entity_t *e, entity_t *other); // 44ee
void player_idle_collide_world(game_state_t *state, entity_t *e);                   // 463f

// 08bc
static const entity_state_t state_player_idle = {
    .sprite_left_idx   = 9,
    .sprite_right_idx  = 1,
    .interactive       = 1,
    .tick_period       = 6,
    .x_step            = 0,
    .y_step            = 0,
    .on_update         = player_idle_update,
    .on_collide_entity = player_idle_collide_entity,
    .on_collide_world  = player_idle_collide_world,
    .next_state        = &state_player_idle,
};

#endif
