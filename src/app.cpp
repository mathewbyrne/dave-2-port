#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ega.cpp"
#include "types.h"

enum {
    GAME_SCREEN_PIXELS      = EGA_SCREEN_WIDTH * EGA_SCREEN_HEIGHT,
    TITLE_FILE_HEADER_BYTES = 8,
    TITLE_FILE_IMAGE_BYTES  = 4 * (GAME_SCREEN_PIXELS / 8),
    TITLE_FILE_BYTES        = TITLE_FILE_HEADER_BYTES + TITLE_FILE_IMAGE_BYTES,

    TITLE_SCROLL_STEP        = 4,
    TITLE_SCROLL_RIGHT_BEGIN = 200,
    TITLE_SCROLL_RIGHT_END   = 280,
    TITLE_SCROLL_LEFT_BEGIN  = 400,
    TITLE_SCROLL_LEFT_END    = 480,
    TITLE_T_MAX              = 480,

    TICK_RATE               = 70,
    FADE_TICK               = 9,
    MENU_OPENING_TICK_COUNT = 8,
};

static const uint8_t g_fade_in_map16[4][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7},
    {0, 0, 0, 0, 0, 0, 0, 0, 24, 25, 26, 27, 28, 29, 30, 31},
    {0, 1, 2, 3, 4, 5, 6, 7, 24, 25, 26, 27, 28, 29, 30, 31},
};

static int load_file(const char *filename, uint8_t **data, size_t *len) {
    FILE *fp;
    if (fopen_s(&fp, filename, "rb") != 0) {
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }

    long file_len = ftell(fp);
    if (file_len < 0) {
        fclose(fp);
        return 1;
    }

    rewind(fp);

    *len  = (size_t)file_len;
    *data = (uint8_t *)malloc(*len);
    if (!*data) {
        fclose(fp);
        return 1;
    }

    if (fread(*data, 1, *len, fp) != *len) {
        free(*data);
        *data = 0;
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

static void decode_title(const char *filename, ega_buffer_t *dst) {
    uint8_t *data = 0;
    size_t   len  = 0;

    assert(load_file(filename, &data, &len) == 0);
    assert(len == TITLE_FILE_BYTES);

    dst->w = EGA_SCREEN_WIDTH;
    dst->h = EGA_SCREEN_HEIGHT;

    ega_decode_4_plane(dst->data, data + TITLE_FILE_HEADER_BYTES,
                       EGA_SCREEN_WIDTH * EGA_SCREEN_HEIGHT / 8, 0);
    free(data);
}

static void load_titles(game_state_t *state) {
    state->title_1 = ega_buffer_alloc(&state->asset_arena, state->width, state->height);
    state->title_2 = ega_buffer_alloc(&state->asset_arena, state->width, state->height);
    decode_title("dos/TITLE1.DD2", state->title_1);
    decode_title("dos/TITLE2.DD2", state->title_2);
}

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

static_assert((EXEC_SPRITE_COUNT * 8) <= EXEC_SPRITE_DATA_STRIDE);

static void load_executable_assets(game_state_t *state) {
    uint8_t *data = 0;
    size_t   len  = 0;

    assert(load_file("dos/DAVE.EXE", &data, &len) == 0);

    const size_t sprite_plane_span = (size_t)EXEC_SPRITE_DATA_STRIDE;
    const size_t sprite_data_end =
        (size_t)EXEC_SPRITE_DATA_OFFSET + (size_t)(EXEC_SPRITE_COUNT * 8) + (3 * sprite_plane_span);
    assert(sprite_data_end <= len);

    uint16_t offset = EXEC_SPRITE_DATA_STRIDE - 8;
    for (int i = 0; i < EXEC_SPRITE_COUNT; i++) {
        state->exec_sprites[i] = ega_buffer_alloc(&state->asset_arena, 8, 8);
        ega_decode_4_plane(state->exec_sprites[i]->data, data + EXEC_SPRITE_DATA_OFFSET + 8 * i, 8,
                           offset);
    }

    free(data);
}

static void fill_rect(ega_buffer_t *dst, int x, int y, int w, int h, uint8_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;

    if (x1 > dst->w) {
        x1 = dst->w;
    }
    if (y1 > dst->h) {
        y1 = dst->h;
    }

    for (int yy = y0; yy < y1; yy++) {
        memset(dst->data + (size_t)yy * dst->stride + (size_t)x0, color, (size_t)(x1 - x0));
    }
}

static void render_title_scroll(game_state_t *state) {
    int scroll_px = (int)state->screen_offset_x;

    ega_buffer_blit(state->buffer, state->title_1, -1 * scroll_px, 0, 0, 0, EGA_SCREEN_WIDTH,
                    EGA_SCREEN_HEIGHT);
    ega_buffer_blit(state->buffer, state->title_2, -1 * scroll_px + EGA_SCREEN_WIDTH, 0, 0, 0,
                    EGA_SCREEN_WIDTH, EGA_SCREEN_HEIGHT);
}

static void render_loading(game_state_t *state) {
    fill_rect(state->buffer, 0, 0, EGA_SCREEN_WIDTH, EGA_SCREEN_HEIGHT, 0);
}

static void render_level(game_state_t *state) {
    // TODO implement level rendering
}

static void render_menu_overlay(game_state_t *state) {
    // TODO implement menu rendering
    ega_buffer_blit(state->buffer, state->exec_sprites[EXEC_SPR_WINDOW_NW], 8, 8, 0, 0, 8, 8);
}

static void set_scene(game_state_t *state, game_scene_t scene) {
    state->scene            = scene;
    state->tick_accumulator = 0.0f;
    state->tick             = 0;
    state->t                = 0;
    state->screen_offset_x  = 0;
}

static void update_scene(game_state_t *state) {
    if (state->scene == GAME_SCENE_LOADING) {
        if (state->loading_step == 0) {
            load_titles(state);
            state->loading_step++;
        } else if (state->loading_step == 1) {
            load_executable_assets(state);
            state->loading_step++;
        } else {
            set_scene(state, GAME_SCENE_TITLE);
        }
        return;
    }

    if (state->scene == GAME_SCENE_TITLE) {
        if (state->tick >= FADE_TICK * 6) {
            if (state->t > TITLE_SCROLL_RIGHT_BEGIN && state->t <= TITLE_SCROLL_RIGHT_END) {
                state->screen_offset_x += TITLE_SCROLL_STEP;
            }

            if (state->t > TITLE_SCROLL_LEFT_BEGIN && state->t <= TITLE_SCROLL_LEFT_END) {
                state->screen_offset_x -= TITLE_SCROLL_STEP;
            }

            if (state->t > TITLE_T_MAX) {
                state->t = 0;
            }
        }

        state->t += 1;
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

    state->width  = EGA_SCREEN_WIDTH;
    state->height = EGA_SCREEN_HEIGHT;
    state->buffer = ega_buffer_alloc(&state->scratch_arena, state->width, state->height);

    state->loading_step_count = 2;
    set_scene(state, GAME_SCENE_LOADING);
}

void game_tick(game_state_t *state, const game_input_t *input, float dt_seconds,
               uint32_t *out_pixels, int out_width, int out_height) {
    game_input_t tick_input = {0};
    if (input) {
        tick_input = *input;
    }

    state->tick_accumulator += dt_seconds;
    while (state->tick_accumulator >= (1.0f / (float)TICK_RATE)) {
        state->tick_accumulator -= (1.0f / (float)TICK_RATE);

        if (tick_input.toggle_pause) {
            if (state->menu_state == GAME_MENU_HIDDEN) {
                state->menu_state      = GAME_MENU_OPENING;
                state->menu_anim_ticks = 0;
            } else {
                state->menu_state = GAME_MENU_HIDDEN;
            }
            tick_input.toggle_pause = 0;
        }

        if (tick_input.next_scene && state->scene != GAME_SCENE_LOADING) {
            if (state->scene == GAME_SCENE_TITLE) {
                set_scene(state, GAME_SCENE_LEVEL);
            } else {
                set_scene(state, GAME_SCENE_TITLE);
            }
            tick_input.next_scene = 0;
        }

        state->tick += 1;
        update_menu(state);

        if (state->menu_state == GAME_MENU_HIDDEN) {
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

    if (state->scene == GAME_SCENE_LOADING) {
        render_loading(state);
    } else if (state->scene == GAME_SCENE_TITLE) {
        render_title_scroll(state);
    } else {
        render_level(state);
    }

    if (state->menu_state != GAME_MENU_HIDDEN) {
        render_menu_overlay(state);
    }

    ega_render_buffer(out_pixels, state->buffer->data, out_width, out_height, pal);
}
