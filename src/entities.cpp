#include "entities.h"

#include <assert.h>
#include <string.h>

void entities_arena_init(entities_arena_t *arena, void *storage, uint16_t cap) {
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

void entities_arena_clear(entities_arena_t *arena) {
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

entity_t *entities_alloc(entities_arena_t *arena, uint16_t type) {
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

void entities_destroy(entities_arena_t *arena, entity_t *entity) {
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
    entity->type   = 0;
}

size_t entities_arena_data_bytes(const entities_arena_t *arena) {
    assert(arena);
    return sizeof(entity_t) * (size_t)arena->cap;
}

size_t entities_storage_bytes(uint16_t cap) {
    return sizeof(entity_t) * (size_t)cap;
}
