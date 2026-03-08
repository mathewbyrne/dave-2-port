#include "entity.h"

#include <assert.h>
#include <string.h>

void entity_arena_init(entity_arena_t *arena, void *storage, uint16_t cap) {
    assert(arena);
    assert(storage || cap == 0);

    arena->items          = (entity_t *)storage;
    arena->cap            = cap;
    arena->count          = 0;
    arena->next_free_hint = 0;

    if (arena->items && cap > 0) {
        memset(arena->items, 0, sizeof(entity_t) * (size_t)cap);
    }
}

void entity_arena_clear(entity_arena_t *arena) {
    assert(arena);
    if (!arena->items || arena->cap == 0) {
        arena->count          = 0;
        arena->next_free_hint = 0;
        return;
    }

    memset(arena->items, 0, sizeof(entity_t) * (size_t)arena->cap);
    arena->count          = 0;
    arena->next_free_hint = 0;
}

entity_t *entity_alloc(entity_arena_t *arena, entity_type_t type) {
    assert(arena);
    assert(type != 0);

    if (!arena->items || arena->cap == 0 || arena->count >= arena->cap) {
        return 0;
    }

    for (uint16_t i = 0; i < arena->cap; i += 1) {
        uint16_t idx = (uint16_t)(arena->next_free_hint + i);
        if (idx >= arena->cap) {
            idx = (uint16_t)(idx - arena->cap);
        }

        entity_t *e = arena->items + idx;
        if (e->type == 0) {
            memset(e, 0, sizeof(*e));
            e->type   = type;
            e->active = 1;
            arena->count += 1;
            arena->next_free_hint = (uint16_t)(idx + 1);
            if (arena->next_free_hint >= arena->cap) {
                arena->next_free_hint = 0;
            }
            return e;
        }
    }

    return 0;
}

void entity_destroy(entity_arena_t *arena, entity_t *entity) {
    assert(arena);
    assert(entity);
    assert(arena->items);
    assert(entity >= arena->items);
    assert(entity < arena->items + arena->cap);

    if (entity->type != 0) {
        if (arena->count > 0) {
            arena->count -= 1;
        }
    }

    entity->active = 0;
    entity->type   = ENTITY_TYPE_FREE;
}

size_t entity_arena_data_bytes(const entity_arena_t *arena) {
    assert(arena);
    return sizeof(entity_t) * (size_t)arena->cap;
}

size_t entity_storage_bytes(uint16_t cap) {
    return sizeof(entity_t) * (size_t)cap;
}

void entity_set_state(game_state_t *state, entity_t *e, const entity_state_t *s) {
    e->state = s;

    if (e->x_dir < 1) {
        e->sprite = &state->assets.sprites[s->sprite_left_idx];
    } else {
        e->sprite = &state->assets.sprites[s->sprite_right_idx];
    }

    if (s->tick_period < e->tick_accum) {
        e->tick_accum = s->tick_period - 1;
    }
}

void player_idle_update(game_state_t *state, entity_t *e) {
}

void player_idle_collide_entity(game_state_t *state, entity_t *e, entity_t *other) {
}

void player_idle_collide_world(game_state_t *state, entity_t *e) {
}
