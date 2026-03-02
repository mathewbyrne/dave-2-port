#ifndef ENTITIES_H
#define ENTITIES_H

#include <stddef.h>
#include <stdint.h>

typedef struct entity_t {
    uint16_t type; // 0 means free slot / destroyed
    int16_t  x;
    int16_t  y;
    uint8_t  direction;
    uint16_t sprite_id;
    uint8_t  active;

    int16_t vx;
    int16_t vy;
} entity_t;

typedef struct entities_arena_t {
    entity_t *items;
    uint16_t  cap;
    uint16_t  count;
    uint16_t  next_free_hint;
} entities_arena_t;

/**
 * entities_arena_init initialises an arena backed by caller-provided storage.
 * storage must point to at least entities_storage_bytes(cap) bytes.
 */
void entities_arena_init(entities_arena_t *arena, void *storage, uint16_t cap);

/**
 * entities_arena_clear clears all entities and resets allocation state.
 */
void entities_arena_clear(entities_arena_t *arena);

/**
 * entities_alloc returns a free entity slot (where type == 0), clears it,
 * assigns type and marks active = 1. Returns 0 if no free slot exists.
 */
entity_t *entities_alloc(entities_arena_t *arena, uint16_t type);

/**
 * entities_destroy clears active and sets type = 0 so the slot can be reused.
 */
void entities_destroy(entities_arena_t *arena, entity_t *entity);

/**
 * entities_arena_data_bytes returns bytes used by the contiguous entity array.
 */
size_t entities_arena_data_bytes(const entities_arena_t *arena);
size_t entities_storage_bytes(uint16_t cap);

static inline entity_t *entities_begin(entities_arena_t *arena) {
    return arena->items;
}

static inline entity_t *entities_end(entities_arena_t *arena) {
    return arena->items + arena->cap;
}

static inline const entity_t *entities_begin_const(const entities_arena_t *arena) {
    return arena->items;
}

static inline const entity_t *entities_end_const(const entities_arena_t *arena) {
    return arena->items + arena->cap;
}

#endif
