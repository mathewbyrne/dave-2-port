#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asset.cpp"
#include "ega.cpp"
#include "entity.cpp"
#include "types.h"

#define WORLD_TO_SCREEN(w, cam) (((w) - (cam)) / 16)

enum {
    TILE_WIDTH  = 16,
    TILE_HEIGHT = 16,

    TITLE_SCROLL_STEP        = 4,
    TITLE_SCROLL_RIGHT_BEGIN = 200,
    TITLE_SCROLL_RIGHT_END   = 280,
    TITLE_SCROLL_LEFT_BEGIN  = 400,
    TITLE_SCROLL_LEFT_END    = 480,
    TITLE_T_MAX              = 480,

    TICK_RATE               = 70,
    FADE_TICK               = 9,
    MENU_OPENING_TICK_COUNT = 8,
    LEVEL_SCROLL_STEP       = 64,
};

static const uint8_t g_fade_in_map16[4][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7},
    {0, 0, 0, 0, 0, 0, 0, 0, 24, 25, 26, 27, 28, 29, 30, 31},
    {0, 1, 2, 3, 4, 5, 6, 7, 24, 25, 26, 27, 28, 29, 30, 31},
};

enum {
    EXEC_SPR_NONE,

    EXEC_SPR_WINDOW_NW,
    EXEC_SPR_WINDOW_N1,
    EXEC_SPR_WINDOW_N2,
    EXEC_SPR_WINDOW_NE,
    EXEC_SPR_WINDOW_W,
    EXEC_SPR_WINDOW_E,
    EXEC_SPR_WINDOW_SW,
    EXEC_SPR_WINDOW_S1,
    EXEC_SPR_WINDOW_S2,
    EXEC_SPR_WINDOW_SE,

    EXEC_SPR_DOT_1,
    EXEC_SPR_DOT_2,
    EXEC_SPR_DOT_3,
    EXEC_SPR_DOT_4,
    EXEC_SPR_DOT_5,
    EXEC_SPR_DOT_6,

    EXEC_SPR_ARROW_N,
    EXEC_SPR_ARROW_NE,
    EXEC_SPR_ARROW_E,
    EXEC_SPR_ARROW_SE,
    EXEC_SPR_ARROW_S,
    EXEC_SPR_ARROW_SW,
    EXEC_SPR_ARROW_W,
    EXEC_SPR_ARROW_NW,

    EXEC_SPR_WINDOW_BG,
    EXEC_SPR_WINDOW_BG_LEN = 7, // 7 consecutive window background sprites (all white)
};

static void draw_char(game_state_t *state, char c, int x, int y) {
    uint8_t idx = (uint8_t)c;
    if (state->assets.glyphs[idx] == 0) {
        return;
    }
    ega_buffer_blit_colour(state->buffer, state->assets.glyphs[(uint8_t)c], 0, x, y);
}

int str_width_pixels(const game_state_t *state, const char *src) {
    int length = 0;

    const char *p = src;
    while (*p) {
        unsigned char c = (unsigned char)*p++;

        // TODO Figure out what this control character does.
        // Is it actually used in any strings anywhere?
        if (c == 0x7f) {
            if (*p) {
                p++; // consume escape payload byte
            }
            continue;
        }

        const ega_buffer_t *g = state->assets.glyphs[(uint8_t)c];
        if (g) {
            length += g->w;
        }
    }

    return length;
}

static void draw_string(game_state_t *state, char *str, int x, int y) {
    while (*str) {
        ega_buffer_t *glyph = state->assets.glyphs[(uint8_t)*str];
        if (glyph) {
            draw_char(state, *str, x, y);
            x += glyph->w;
        }
        str++;
    }
}

static void draw_string_center(game_state_t *state, char *str, int y) {
    int len = str_width_pixels(state, str);
    int x   = (state->buffer->w - len) / 2;
    draw_string(state, str, x, y);
}

static void fill_rect(ega_buffer_t *dst, int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;

    if (x1 <= 0 || y1 <= 0 || x0 >= dst->w || y0 >= dst->h) {
        return;
    }

    if (x1 > dst->w) {
        x1 = dst->w;
    }
    if (y1 > dst->h) {
        y1 = dst->h;
    }

    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int yy = y0; yy < y1; yy++) {
        memset(dst->data + (size_t)yy * dst->stride + (size_t)x0, color, (size_t)(x1 - x0));
    }
}

static void render_title_scroll(game_state_t *state) {
    int scroll_px = (int)state->screen_offset_x;

    ega_buffer_blit(state->buffer, state->assets.titles[0], -1 * scroll_px, 0, 0, 0,
                    EGA_SCREEN_WIDTH, EGA_SCREEN_HEIGHT);
    ega_buffer_blit(state->buffer, state->assets.titles[1], -1 * scroll_px + EGA_SCREEN_WIDTH, 0, 0,
                    0, EGA_SCREEN_WIDTH, EGA_SCREEN_HEIGHT);
}

static void camera_reset(game_state_t *state) {
    uint16_t camera_x = state->player->x - 0xa00;
    uint16_t camera_y = state->player->y - 0x700;

    if ((camera_x < state->level_min_camera_x) || (state->player->x < 0xa00)) {
        camera_x = state->level_min_camera_x;
    }
    if ((camera_y < state->level_min_camera_y) || (state->player->y < 0x700)) {
        camera_y = state->level_min_camera_y;
    }
    if (state->level_max_camera_x < camera_x) {
        camera_x = state->level_max_camera_x;
    }
    if (state->level_max_camera_y < camera_y) {
        camera_y = state->level_max_camera_y;
    }

    state->level_camera_x = camera_x;
    state->level_camera_y = camera_y;
    // TODO: for when we need screen tile offsets cached
    // g_screen_offset_tile_x = screen_offset_x >> 8 & 0xfe;
    // g_screen_offset_tile_y = screen_offset_y >> 8 & 0xfe;
}

static void render_level(game_state_t *state) {
    int cam_x = (int)state->level_camera_x >> 4;
    int cam_y = (int)state->level_camera_y >> 4;

    fill_rect(state->buffer, 0, 0, EGA_SCREEN_WIDTH, EGA_SCREEN_HEIGHT, 0);

    int first_tile_x = cam_x / TILE_WIDTH;
    int first_tile_y = cam_y / TILE_HEIGHT;
    int offset_x     = -(cam_x % TILE_WIDTH);
    int offset_y     = -(cam_y % TILE_HEIGHT);

    int tiles_x = (EGA_SCREEN_WIDTH / TILE_WIDTH) + 2;
    int tiles_y = (EGA_SCREEN_HEIGHT / TILE_HEIGHT) + 2;

    for (int ty = 0; ty < tiles_y; ty++) {
        for (int tx = 0; tx < tiles_x; tx++) {
            int map_x = first_tile_x + tx;
            int map_y = first_tile_y + ty;
            if (map_x < 0 || map_y < 0 || map_x >= state->level.w || map_y >= state->level.h) {
                continue;
            }

            int      draw_x   = offset_x + tx * TILE_WIDTH;
            int      draw_y   = offset_y + ty * TILE_HEIGHT;
            uint16_t tile_idx = state->level.tiles[map_x][map_y];
            if (tile_idx < ASSET_TILES_COUNT && state->assets.tiles[tile_idx].gfx) {
                ega_buffer_blit(state->buffer, state->assets.tiles[tile_idx].gfx, draw_x, draw_y, 0,
                                0, TILE_WIDTH, TILE_HEIGHT);
            }
        }
    }

    // for (int i = 0; i < state->entity_arena.count; i++) {
    // }
    ega_buffer_blit_masked(state->buffer, state->assets.sprites[1].image,
                           state->assets.sprites[1].mask,
                           WORLD_TO_SCREEN(state->player->x, state->level_camera_x),
                           WORLD_TO_SCREEN(state->player->y, state->level_camera_y), 0, 0,
                           state->assets.sprites[1].w, state->assets.sprites[1].h);
}

static void render_menu_overlay(game_state_t *state) {
    // TODO implement menu rendering

    ega_buffer_blit(state->buffer, state->assets.ui_sprites[EXEC_SPR_WINDOW_NW], 8, 8, 0, 0, 8, 8);

    draw_string_center(state, (char *)"RESET GAME (Y/N)?", 16);

    // fill_rect(state->buffer, 2, 2, 316, 196, 0xf);
    // int x = 8;
    // int y = 8;
    // for (int i = 0; i < 16; i++) {
    //     for (int j = 0; j < 16; j++) {
    //         draw_char(state, i * 16 + j, x, y);
    //         x += 10;
    //     }
    //     x = 8;
    //     y += 10;
    // }
    //
    // draw_string(state, "abcdefghijklmnopqrstuvwxyz", 8, 140);
    // draw_string(state, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 8, 150);
    //
}

static void level_load(game_state_t *state) {
    state->level_min_camera_x = 0x200;
    state->level_min_camera_y = 0x200;
    state->level_max_camera_x = (state->level.w + -0x16) * 0x100;
    state->level_max_camera_y = (state->level.h + -0xf) * 0x100;

    for (int tile_y = 0; tile_y < state->level.h; tile_y++) {
        for (int tile_x = 0; tile_x < state->level.w; tile_x++) {
            switch (state->level.tags[tile_x][tile_y]) {
            case LEVEL_TAG_PLAYER:
                state->player->dir    = 1;
                state->player->active = 1;
                state->player->x      = tile_x << 8;
                state->player->y      = tile_y * 0x100 + 0xf;
                // entity_set_state(state->player, player_state_standing)
            }
        }
    }
}

static void set_scene(game_state_t *state, game_scene_t scene) {
    state->scene            = scene;
    state->tick_accumulator = 0.0f;
    state->tick             = 0;
    state->screen_offset_x  = 0;

    switch (scene) {
    case GAME_SCENE_LEVEL:
        state->level_current++;
        state->player_score = 0;
        state->player_lives = 4;
        state->player_ammo  = 8;

        state->player = entity_alloc(&state->entity_arena, ENTITY_TYPE_PLAYER);

        level_load(state);

        camera_reset(state);
    }
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static game_direction_t input_direction_from_keys(const game_direction_state_t *keys) {
    int dx = (int)keys->move_right - (int)keys->move_left;
    int dy = (int)keys->move_down - (int)keys->move_up;

    if (dx == 0 && dy == 0) {
        return GAME_DIRECTION_NONE;
    }
    if (dx == 0 && dy < 0) {
        return GAME_DIRECTION_N;
    }
    if (dx > 0 && dy < 0) {
        return GAME_DIRECTION_NE;
    }
    if (dx > 0 && dy == 0) {
        return GAME_DIRECTION_E;
    }
    if (dx > 0 && dy > 0) {
        return GAME_DIRECTION_SE;
    }
    if (dx == 0 && dy > 0) {
        return GAME_DIRECTION_S;
    }
    if (dx < 0 && dy > 0) {
        return GAME_DIRECTION_SW;
    }
    if (dx < 0 && dy == 0) {
        return GAME_DIRECTION_W;
    }
    return GAME_DIRECTION_NW;
}

static void update_level(game_state_t *state, const game_input_t *input) {
    int cam_x = (int)state->level_camera_x;
    int cam_y = (int)state->level_camera_y;
    if (input->direction == GAME_DIRECTION_W || input->direction == GAME_DIRECTION_NW ||
        input->direction == GAME_DIRECTION_SW) {
        cam_x -= LEVEL_SCROLL_STEP;
    }
    if (input->direction == GAME_DIRECTION_E || input->direction == GAME_DIRECTION_NE ||
        input->direction == GAME_DIRECTION_SE) {
        cam_x += LEVEL_SCROLL_STEP;
    }
    if (input->direction == GAME_DIRECTION_N || input->direction == GAME_DIRECTION_NE ||
        input->direction == GAME_DIRECTION_NW) {
        cam_y -= LEVEL_SCROLL_STEP;
    }
    if (input->direction == GAME_DIRECTION_S || input->direction == GAME_DIRECTION_SE ||
        input->direction == GAME_DIRECTION_SW) {
        cam_y += LEVEL_SCROLL_STEP;
    }

    state->level_camera_x =
        (uint16_t)clampi(cam_x, state->level_min_camera_x, state->level_max_camera_x);
    state->level_camera_y =
        (uint16_t)clampi(cam_y, state->level_min_camera_y, state->level_max_camera_y);
}

static void update_scene(game_state_t *state) {
    if (state->scene == GAME_SCENE_TITLE) {
        if (state->tick >= FADE_TICK * 6) {
            if (state->tick > TITLE_SCROLL_RIGHT_BEGIN && state->tick <= TITLE_SCROLL_RIGHT_END) {
                state->screen_offset_x += TITLE_SCROLL_STEP;
            }

            if (state->tick > TITLE_SCROLL_LEFT_BEGIN && state->tick <= TITLE_SCROLL_LEFT_END) {
                state->screen_offset_x -= TITLE_SCROLL_STEP;
            }

            if (state->tick > TITLE_T_MAX) {
                state->tick = 0;
            }
        }

        state->tick += 1;
    }
}

static void update_menu(game_state_t *state) {
    if (state->menu_state == GAME_MENU_OPENING) {
        state->menu_anim_ticks += 1;
        if (state->menu_anim_ticks >= MENU_OPENING_TICK_COUNT) {
            state->menu_state = GAME_MENU_OPEN;
        }
    }
}

void game_init(game_state_t *state) {
    memset(state, 0, sizeof(*state));

    ega_arena_init(&state->asset_arena, &state->asset_mem, GAME_STATE_ASSET_CAP);
    ega_arena_init(&state->scratch_arena, &state->scratch_mem, GAME_STATE_SCRATCH_CAP);
    entity_arena_init(&state->entity_arena, state->entity_mem, GAME_ENTITY_CAP);

    state->width  = EGA_SCREEN_WIDTH;
    state->height = EGA_SCREEN_HEIGHT;
    state->buffer = ega_buffer_alloc(&state->scratch_arena, state->width, state->height);

    asset_load_shared(&state->assets, "dos", &state->asset_arena);
    asset_load_level(&state->level, "LEVEL01.DD2", "dos");
}

void game_tick(game_state_t *state, const game_input_t *input, float dt_seconds,
               uint32_t *out_pixels, int out_width, int out_height) {
    game_input_t tick_input = {};
    if (input) {
        tick_input = *input;
    }
    tick_input.direction = input_direction_from_keys(&tick_input.direction_keys);

    state->tick_accumulator += dt_seconds;
    while (state->tick_accumulator >= (1.0f / (float)TICK_RATE)) {
        state->tick_accumulator -= (1.0f / (float)TICK_RATE);

        if (state->scene == GAME_SCENE_TITLE &&
            (tick_input.direction != GAME_DIRECTION_NONE ||
             tick_input.action_1.pressed_this_frame || tick_input.action_2.pressed_this_frame)) {
            set_scene(state, GAME_SCENE_LEVEL);
        }

        if (tick_input.menu_status) {
            if (state->menu_state == GAME_MENU_HIDDEN) {
                state->menu_state      = GAME_MENU_OPENING;
                state->menu_anim_ticks = 0;
            } else {
                state->menu_state = GAME_MENU_HIDDEN;
            }
            tick_input.menu_status = 0;
        }

        state->tick += 1;
        update_menu(state);

        if (state->menu_state == GAME_MENU_HIDDEN) {
            if (state->scene == GAME_SCENE_LEVEL) {
                update_level(state, &tick_input);
            }
            update_scene(state);
        }
    }

    const uint8_t *pal = g_fade_in_map16[3];
    if (state->scene == GAME_SCENE_TITLE) {
        if (state->tick < FADE_TICK * 3) {
            pal = g_fade_in_map16[0];
        } else if (state->tick < FADE_TICK * 4) {
            pal = g_fade_in_map16[1];
        } else if (state->tick < FADE_TICK * 5) {
            pal = g_fade_in_map16[2];
        }
    }

    if (state->scene == GAME_SCENE_TITLE) {
        render_title_scroll(state);
    } else {
        render_level(state);
    }

    if (state->menu_state != GAME_MENU_HIDDEN) {
        render_menu_overlay(state);
    }

    ega_render_buffer(out_pixels, state->buffer->data, out_width, out_height, pal);
}
