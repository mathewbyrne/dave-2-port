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

typedef struct entity_t {
    entity_type_t type; // 0 means free slot / destroyed
    int16_t       x;
    int16_t       y;
    uint8_t       dir;
    uint16_t      sprite_id;
    uint8_t       active;

    int16_t vx;
    int16_t vy;
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

#endif
