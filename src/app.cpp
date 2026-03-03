#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decode.cpp"
#include "ega.cpp"
#include "entities.cpp"
#include "types.h"

enum {
    GAME_SCREEN_PIXELS      = EGA_SCREEN_WIDTH * EGA_SCREEN_HEIGHT,
    TITLE_FILE_HEADER_BYTES = 8,
    TITLE_FILE_IMAGE_BYTES  = 4 * (GAME_SCREEN_PIXELS / 8),
    TITLE_FILE_BYTES        = TITLE_FILE_HEADER_BYTES + TITLE_FILE_IMAGE_BYTES,

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
    LEVEL_SCROLL_STEP       = 4,
};

static const uint8_t g_fade_in_map16[4][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7},
    {0, 0, 0, 0, 0, 0, 0, 0, 24, 25, 26, 27, 28, 29, 30, 31},
    {0, 1, 2, 3, 4, 5, 6, 7, 24, 25, 26, 27, 28, 29, 30, 31},
};

static int load_file(const char *filename, uint8_t **data, size_t *len) {
    FILE *fp = 0;
#if defined(_MSC_VER)
    if (fopen_s(&fp, filename, "rb") != 0) {
        return 1;
    }
#else
    fp = fopen(filename, "rb");
    if (!fp) {
        return 1;
    }
#endif

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

static uint16_t read_le_u16(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static int16_t read_le_i16(const uint8_t *src) {
    return (int16_t)read_le_u16(src);
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

static void load_tiles(game_state_t *state) {
    uint8_t *data = 0;
    size_t   len  = 0;
    assert(load_file("dos/EGATILES.DD2", &data, &len) == 0);

    uint16_t stride = TILE_WIDTH * TILE_HEIGHT / 8;
    assert(len == stride * 4 * GAME_TILES_COUNT);

    for (uint16_t i = 0; (i + 1) * stride * 4 < len; i += 1) {
        state->tiles[i] = ega_buffer_alloc(&state->asset_arena, TILE_WIDTH, TILE_HEIGHT);
        ega_decode_4_plane(state->tiles[i]->data, data + i * stride * 4, stride, 0);
    }

    free(data);
}

typedef struct {
    const char *filename;
    uint16_t    first_sprite_id;
    uint16_t    sprite_count;
} sprite_file_info_t;

static const sprite_file_info_t g_sprite_files[] = {
    {"dos/S_DAVE.DD2", 0x00, 85},  {"dos/S_CHUNK1.DD2", 0x55, 36}, {"dos/S_CHUNK2.DD2", 0x79, 53},
    {"dos/S_FRANK.DD2", 0xAE, 13}, {"dos/S_MASTER.DD2", 0xBB, 12},
};

static_assert((85 + 36 + 53 + 13 + 12) == EXEC_SPR_DEF_GROUP_COUNT);

static void load_sprites(game_state_t *state) {
    for (size_t f = 0; f < (sizeof(g_sprite_files) / sizeof(g_sprite_files[0])); f++) {
        const sprite_file_info_t *file_info = &g_sprite_files[f];

        uint8_t *encode_data = 0;
        size_t   encode_len  = 0;
        assert(load_file(file_info->filename, &encode_data, &encode_len) == 0);

        huff_src src = {encode_data, encode_len};
        size_t   decode_len;
        assert(huff_len(&decode_len, src) == HUFF_OK);

        uint8_t *decode_data = (uint8_t *)malloc(decode_len);
        assert(decode_data != 0);
        assert(huff_decode(decode_data, decode_len, src) == HUFF_OK);
        free(encode_data);

        uint16_t chunk_size = read_le_u16(decode_data + 0x00);
        assert((size_t)chunk_size * 5 + 2 <= decode_len);

        const uint8_t *plane_data = decode_data + 0x02;
        const uint8_t *mask_data  = decode_data + 0x02 + ((size_t)chunk_size * 4);

        for (int i = 0; i < file_info->sprite_count; i++) {
            uint16_t sprite_id = (uint16_t)(file_info->first_sprite_id + i);
            assert(sprite_id < EXEC_SPR_DEF_GROUP_COUNT);

            const exec_sprite_def_record_t *def = &state->exec_sprite_defs[sprite_id].phase[0];
            if (def->stride_bytes == 0 || def->height == 0) {
                continue;
            }

            uint16_t w = (uint16_t)(def->stride_bytes * 8);
            uint16_t h = def->height;

            size_t sprite_len = (size_t)def->stride_bytes * (size_t)def->height;
            size_t sprite_off = (size_t)def->unknown_0x06 * 16 + (size_t)def->unknown_0x04;
            assert(sprite_len <= chunk_size);
            assert((sprite_off + sprite_len) <= chunk_size);

            state->sprites[sprite_id].image = ega_buffer_alloc(&state->asset_arena, w, h);
            state->sprites[sprite_id].mask  = ega_buffer_alloc(&state->asset_arena, w, h);
            state->sprites[sprite_id].w     = w;
            state->sprites[sprite_id].h     = h;

            uint16_t plane_offset = (uint16_t)(chunk_size - sprite_len);
            ega_decode_4_plane(state->sprites[sprite_id].image->data, plane_data + sprite_off,
                               (uint16_t)sprite_len, plane_offset);
            ega_decode_1bpp(state->sprites[sprite_id].mask->data, mask_data + sprite_off, w, h);
        }

        free(decode_data);
    }
}

static void load_level(game_state_t *state, const char *filename) {
    uint8_t *data = 0;
    size_t   len  = 0;
    assert(load_file(filename, &data, &len) == 0);

    size_t decode_len = 0;
    assert(rlew_len(&decode_len, data, len) == 0);

    uint8_t *decode_data = (uint8_t *)malloc(decode_len);
    assert(rlew_decode(decode_data, decode_len, data, len) == 0);

    // TODO do something with level data
    uint16_t width        = *(uint16_t *)(decode_data + 0x00);
    uint16_t height       = *(uint16_t *)(decode_data + 0x02);
    uint16_t plane_count  = *(uint16_t *)(decode_data + 0x04);
    uint16_t plane_stride = *(uint16_t *)(decode_data + 0x0E);
    size_t   plane0_off   = 0x20;
    size_t   plane1_off   = plane0_off + plane_stride;

    assert(width <= GAME_LEVEL_MAX_W);
    assert(height <= GAME_LEVEL_MAX_H);
    assert(plane_count >= 2);
    assert((plane1_off + plane_stride) <= decode_len);

    state->level_w = width;
    state->level_h = height;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx                = (size_t)y * (size_t)width + (size_t)x; // word index
            state->level_tiles[x][y]  = *(uint16_t *)(decode_data + plane0_off + idx * 2);
            state->level_plane2[x][y] = *(uint16_t *)(decode_data + plane1_off + idx * 2);
        }
    }

    free(data);
    free(decode_data);
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

    const size_t sprite_def_data_end =
        (size_t)EXEC_SPR_DEF_OFFSET +
        ((size_t)EXEC_SPR_DEF_RECORD_COUNT * EXEC_SPR_DEF_RECORD_SIZE);
    assert(sprite_def_data_end <= len);

    for (int group_i = 0; group_i < EXEC_SPR_DEF_GROUP_COUNT; group_i++) {
        exec_sprite_def_group_t *group = &state->exec_sprite_defs[group_i];

        for (int phase_i = 0; phase_i < EXEC_SPR_DEF_PHASE_COUNT; phase_i++) {
            size_t record_i   = (size_t)group_i * EXEC_SPR_DEF_PHASE_COUNT + (size_t)phase_i;
            size_t record_off = (size_t)EXEC_SPR_DEF_OFFSET + record_i * EXEC_SPR_DEF_RECORD_SIZE;

            exec_sprite_def_record_t *record = &group->phase[phase_i];
            record->stride_bytes             = read_le_u16(data + record_off + 0x00);
            record->height                   = read_le_u16(data + record_off + 0x02);
            record->unknown_0x04             = read_le_u16(data + record_off + 0x04);
            record->unknown_0x06             = read_le_u16(data + record_off + 0x06);
            record->bbox_dx0                 = read_le_i16(data + record_off + 0x08);
            record->bbox_dy0                 = read_le_i16(data + record_off + 0x0A);
            record->bbox_dx1                 = read_le_i16(data + record_off + 0x0C);
            record->bbox_dy1                 = read_le_i16(data + record_off + 0x0E);
            memcpy(record->name, data + record_off + 0x10, 12);
            record->name[12] = 0;
            record->spawn_dx = read_le_i16(data + record_off + 0x1C);
            record->spawn_dy = read_le_i16(data + record_off + 0x1E);

            if (phase_i == 0) {
                memcpy(group->name, record->name, sizeof(group->name));
            } else {
                assert(memcmp(group->name, record->name, 12) == 0);
            }
        }
    }

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

    uint16_t char_h = *(uint16_t *)(data + EXEC_FONT_SEG_OFFSET);
    for (int i = 0; i < 256; i++) {
        uint16_t char_offset = *(uint16_t *)(data + EXEC_FONT_OFFSETS_OFFSET + i * 2);
        // Some characters just don't have sprite mappings.  Let's leave these as NULL for the
        // time being, however maybe we can just have an empty sprite we assign for these?  A
        // buffer with w = 0, h = 0 should render ok via the blitter.
        if (char_offset == 0) {
            continue;
        }
        uint8_t  char_w        = *(uint8_t *)(data + EXEC_FONT_WIDTHS_OFFSET + i);
        uint16_t bytes_per_row = (char_w + 7) >> 3;
        uint16_t block_size    = bytes_per_row * char_h;

        state->glyphs[i] = ega_buffer_alloc(&state->asset_arena, char_w, char_h);
        ega_decode_1bpp(state->glyphs[i]->data,
                        data + EXEC_FONT_SEG_OFFSET + char_offset + block_size, char_w, char_h);
    }

    free(data);
}

static void draw_char(game_state_t *state, char c, int x, int y) {
    uint8_t idx = (uint8_t)c;
    if (state->glyphs[idx] == 0) {
        return;
    }
    ega_buffer_blit_colour(state->buffer, state->glyphs[(uint8_t)c], 0, x, y);
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

        const ega_buffer_t *g = state->glyphs[(uint8_t)c];
        if (g) {
            length += g->w;
        }
    }

    return length;
}

static void draw_string(game_state_t *state, char *str, int x, int y) {
    while (*str) {
        ega_buffer_t *glyph = state->glyphs[(uint8_t)*str];
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
    int cam_x = (int)state->level_camera_x;
    int cam_y = (int)state->level_camera_y;

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
            if (map_x < 0 || map_y < 0 || map_x >= state->level_w || map_y >= state->level_h) {
                continue;
            }

            int      draw_x   = offset_x + tx * TILE_WIDTH;
            int      draw_y   = offset_y + ty * TILE_HEIGHT;
            uint16_t tile_idx = state->level_tiles[map_x][map_y];
            if (tile_idx < GAME_TILES_COUNT && state->tiles[tile_idx]) {
                ega_buffer_blit(state->buffer, state->tiles[tile_idx], draw_x, draw_y, 0, 0,
                                TILE_WIDTH, TILE_HEIGHT);
            }
        }
    }
}

static void render_menu_overlay(game_state_t *state) {
    // TODO implement menu rendering

    ega_buffer_blit(state->buffer, state->exec_sprites[EXEC_SPR_WINDOW_NW], 8, 8, 0, 0, 8, 8);

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

static void set_scene(game_state_t *state, game_scene_t scene) {
    state->scene            = scene;
    state->tick_accumulator = 0.0f;
    state->tick             = 0;
    state->t                = 0;
    state->screen_offset_x  = 0;
    state->level_camera_x   = 0;
    state->level_camera_y   = 0;
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

static void update_level(game_state_t *state, const game_input_t *input) {
    int max_camera_x = state->level_w * TILE_WIDTH - EGA_SCREEN_WIDTH;
    int max_camera_y = state->level_h * TILE_HEIGHT - EGA_SCREEN_HEIGHT;
    if (max_camera_x < 0) {
        max_camera_x = 0;
    }
    if (max_camera_y < 0) {
        max_camera_y = 0;
    }

    int cam_x = (int)state->level_camera_x;
    int cam_y = (int)state->level_camera_y;
    if (input->move_left) {
        cam_x -= LEVEL_SCROLL_STEP;
    }
    if (input->move_right) {
        cam_x += LEVEL_SCROLL_STEP;
    }
    if (input->move_up) {
        cam_y -= LEVEL_SCROLL_STEP;
    }
    if (input->move_down) {
        cam_y += LEVEL_SCROLL_STEP;
    }

    state->level_camera_x = (uint16_t)clampi(cam_x, 0, max_camera_x);
    state->level_camera_y = (uint16_t)clampi(cam_y, 0, max_camera_y);
}

static void update_scene(game_state_t *state) {
    if (state->scene == GAME_SCENE_LOADING) {
        if (state->loading_step == 0) {
            load_titles(state);
            state->loading_step++;
        } else if (state->loading_step == 1) {
            load_executable_assets(state);
            load_sprites(state);
            load_tiles(state);
            load_level(state, "dos/LEVEL01.DD2");
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
    entities_arena_init(&state->entities, state->entities_mem, GAME_ENTITY_CAP);

    state->width  = EGA_SCREEN_WIDTH;
    state->height = EGA_SCREEN_HEIGHT;
    state->buffer = ega_buffer_alloc(&state->scratch_arena, state->width, state->height);

    state->loading_step_count = 2;
    set_scene(state, GAME_SCENE_LOADING);
}

void game_tick(game_state_t *state, const game_input_t *input, float dt_seconds,
               uint32_t *out_pixels, int out_width, int out_height) {
    game_input_t tick_input = {};
    if (input) {
        tick_input = *input;
    }

    state->tick_accumulator += dt_seconds;
    while (state->tick_accumulator >= (1.0f / (float)TICK_RATE)) {
        state->tick_accumulator -= (1.0f / (float)TICK_RATE);

        if (state->scene == GAME_SCENE_TITLE && (tick_input.move_left || tick_input.move_right ||
                                                 tick_input.move_up || tick_input.move_down)) {
            tick_input.next_scene = 1;
        }

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
