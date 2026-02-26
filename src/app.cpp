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

    ASSERT(load_file(filename, &data, &len) == 0);
    ASSERT(len == TITLE_FILE_BYTES);

    dst->w = EGA_SCREEN_WIDTH;
    dst->h = EGA_SCREEN_HEIGHT;

    // TODO implement ega decode with ega_buffer_t
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

// TODO Ok these offsets are going to be brittle, depending on how someone is
// able to uncompress the executable.  What we do probably know is that they
// will be paragraph aligned since they will be at the start of a segment.
#define EXEC_SPRITE_DATA_OFFSET 0x18970
#define EXEC_SPRITE_DATA_STRIDE 0x33B0

void load_executable_assets(game_state_t *state) {
    uint8_t *data = 0;
    size_t   len  = 0;

    ASSERT(load_file("dos/DAVE.EXE", &data, &len) == 0);

    // Load assets contained in the binary

    free(data);
}

void game_init(game_state_t *state) {
    memset(state, 0, sizeof(*state));

    ega_arena_init(&state->asset_arena, &state->asset_mem, GAME_STATE_ASSET_CAP);
    ega_arena_init(&state->scratch_arena, &state->scratch_mem, GAME_STATE_SCRATCH_CAP);

    state->width  = EGA_SCREEN_WIDTH;
    state->height = EGA_SCREEN_HEIGHT;
    state->buffer = ega_buffer_alloc(&state->scratch_arena, state->width, state->height);

    load_titles(state);
    //    load_executable_assets(state);
}

static void render_title_scroll(game_state_t *state) {
    int scroll_px = (int)state->screen_offset_x;

    ega_buffer_blit(state->buffer, state->title_1, -1 * scroll_px, 0, 0, 0, EGA_SCREEN_WIDTH,
                    EGA_SCREEN_HEIGHT);
    ega_buffer_blit(state->buffer, state->title_2, -1 * scroll_px + EGA_SCREEN_WIDTH, 0, 0, 0,
                    EGA_SCREEN_WIDTH, EGA_SCREEN_HEIGHT);
}

#define FADE_TICK 9

void game_tick(game_state_t *state, float dt_seconds, uint32_t *out_pixels, int out_width,
               int out_height) {
    state->tick_accumulator += dt_seconds;
    while (state->tick_accumulator >= (1.0f / 70.0f)) {
        state->tick_accumulator -= (1.0f / 70.0f);

        state->tick += 1;
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

    const uint8_t *pal;
    if (state->tick < FADE_TICK * 3) {
        pal = g_fade_in_map16[0];
    } else if (state->tick < FADE_TICK * 4) {
        pal = g_fade_in_map16[1];
    } else if (state->tick < FADE_TICK * 5) {
        pal = g_fade_in_map16[2];
    } else {
        pal = g_fade_in_map16[3];
    }

    render_title_scroll(state);
    ega_render_buffer(out_pixels, state->buffer->data, out_width, out_height, pal);
}
