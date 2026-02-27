#include "ega.h"

#include <string.h>

/**
 * ega_align_up takes a size and alignment and returns the lowest multiple of
 * alignment >= size.
 */
static size_t ega_align_up(size_t v, size_t align) {
    EGA_ASSERT(align != 0);
    EGA_ASSERT((align & (align - 1)) == 0); // ensure align is pow 2
    return (v + (align - 1)) & ~(align - 1);
}

void ega_arena_init(ega_arena_t *a, void *storage, size_t cap) {
    a->base = (uint8_t *)storage;
    a->cap  = cap;
    a->off  = 0;
}

void ega_arena_clear(ega_arena_t *a) {
    a->off = 0;
}

void *ega_arena_alloc(ega_arena_t *a, size_t size, size_t align) {
    size_t off = ega_align_up(a->off, align);
    EGA_ASSERT(off <= a->cap);
    EGA_ASSERT(size <= (a->cap - off));
    void *p = a->base + off;
    a->off  = off + size;
    return p;
}

size_t ega_arena_mark(ega_arena_t *a) {
    return a->off;
}

void ega_arena_reset(ega_arena_t *a, size_t mark) {
    EGA_ASSERT(mark <= a->off);
    a->off = mark;
}

ega_buffer_t *ega_buffer_alloc(ega_arena_t *a, uint16_t w, uint16_t h) {
    return ega_buffer_alloc_stride(a, w, h, w);
}

ega_buffer_t *ega_buffer_alloc_stride(ega_arena_t *a, uint16_t w, uint16_t h, uint16_t stride) {
    EGA_ASSERT(stride >= w);

    size_t bytes = (size_t)stride * (size_t)h;
    size_t total = sizeof(ega_buffer_t) + bytes - 1; // -1 due to the flexible array member

    ega_buffer_t *b = (ega_buffer_t *)ega_arena_alloc(a, total, EGA_ALIGNOF(ega_buffer_t));
    b->w            = w;
    b->h            = h;
    b->stride       = stride;

    memset(b->data, 0, bytes);
    return b;
}

size_t ega_buffer_data_bytes(const ega_buffer_t *b) {
    return (size_t)b->stride * (size_t)b->h;
}

void ega_buffer_blit(ega_buffer_t *dst, const ega_buffer_t *src, int dst_x, int dst_y, int src_x,
                     int src_y, int w, int h) {
    assert(dst);
    assert(src);

    if (w <= 0 || h <= 0) {
        return;
    }

    const int dst_w = (int)dst->w;
    const int dst_h = (int)dst->h;
    const int src_w = (int)src->w;
    const int src_h = (int)src->h;

    // Clip against destination (left/top)
    if (dst_x < 0) {
        int d = -dst_x;
        if (d >= w)
            return;
        dst_x = 0;
        src_x += d;
        w -= d;
    }
    if (dst_y < 0) {
        int d = -dst_y;
        if (d >= h)
            return;
        dst_y = 0;
        src_y += d;
        h -= d;
    }

    // Clip against source (left/top)
    if (src_x < 0) {
        int d = -src_x;
        if (d >= w)
            return;
        src_x = 0;
        dst_x += d;
        w -= d;
    }
    if (src_y < 0) {
        int d = -src_y;
        if (d >= h)
            return;
        src_y = 0;
        dst_y += d;
        h -= d;
    }

    // Clip against destination (right/bottom)
    if (dst_x >= dst_w || dst_y >= dst_h) {
        return;
    }
    if (dst_x + w > dst_w) {
        w = dst_w - dst_x;
        if (w <= 0)
            return;
    }
    if (dst_y + h > dst_h) {
        h = dst_h - dst_y;
        if (h <= 0)
            return;
    }

    // Clip against source (right/bottom)
    if (src_x >= src_w || src_y >= src_h) {
        return;
    }
    if (src_x + w > src_w) {
        w = src_w - src_x;
        if (w <= 0)
            return;
    }
    if (src_y + h > src_h) {
        h = src_h - src_y;
        if (h <= 0)
            return;
    }

    uint8_t       *dst_base = dst->data + (size_t)dst_y * (size_t)dst->stride + (size_t)dst_x;
    const uint8_t *src_base = src->data + (size_t)src_y * (size_t)src->stride + (size_t)src_x;

    const size_t row_bytes = (size_t)w;
    const size_t dst_row   = (size_t)dst->stride;
    const size_t src_row   = (size_t)src->stride;

    // Special case here to deal with copying within the same buffer.
    if (dst->data == src->data) {
        if (dst_base < src_base) {
            for (int y = 0; y < h; y += 1) {
                memmove(dst_base + (size_t)y * dst_row, src_base + (size_t)y * src_row, row_bytes);
            }
        } else if (dst_base > src_base) {
            for (int y = h - 1; y >= 0; y -= 1) {
                memmove(dst_base + (size_t)y * dst_row, src_base + (size_t)y * src_row, row_bytes);
            }
        } else {
            // noop, src and dst refer to the same memory.
        }
    } else {
        for (int y = 0; y < h; y += 1) {
            memcpy(dst_base + (size_t)y * dst_row, src_base + (size_t)y * src_row, row_bytes);
        }
    }
}

void ega_buffer_blit_masked(ega_buffer_t *dst, const ega_buffer_t *src, const ega_buffer_t *mask,
                            int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
    assert(dst);
    assert(src);
    assert(mask);

    if (w <= 0 || h <= 0) {
        return;
    }

    // Debug sanity (you can relax this later if you want "mask can be smaller").
    assert(mask->w == src->w);
    assert(mask->h == src->h);

    const int dst_w = (int)dst->w;
    const int dst_h = (int)dst->h;

    // Clip source bounds against BOTH src and mask (so we never read mask OOB even if asserts are
    // off).
    const int src_w = (int)src->w;
    const int src_h = (int)src->h;
    const int msk_w = (int)mask->w;
    const int msk_h = (int)mask->h;
    const int sm_w  = (src_w < msk_w) ? src_w : msk_w;
    const int sm_h  = (src_h < msk_h) ? src_h : msk_h;

    // Clip against destination (left/top)
    if (dst_x < 0) {
        int d = -dst_x;
        if (d >= w)
            return;
        dst_x = 0;
        src_x += d;
        w -= d;
    }
    if (dst_y < 0) {
        int d = -dst_y;
        if (d >= h)
            return;
        dst_y = 0;
        src_y += d;
        h -= d;
    }

    // Clip against source/mask (left/top)
    if (src_x < 0) {
        int d = -src_x;
        if (d >= w)
            return;
        src_x = 0;
        dst_x += d;
        w -= d;
    }
    if (src_y < 0) {
        int d = -src_y;
        if (d >= h)
            return;
        src_y = 0;
        dst_y += d;
        h -= d;
    }

    // Clip against destination (right/bottom)
    if (dst_x >= dst_w || dst_y >= dst_h) {
        return;
    }
    if (dst_x + w > dst_w) {
        w = dst_w - dst_x;
        if (w <= 0)
            return;
    }
    if (dst_y + h > dst_h) {
        h = dst_h - dst_y;
        if (h <= 0)
            return;
    }

    // Clip against source/mask (right/bottom)
    if (src_x >= sm_w || src_y >= sm_h) {
        return;
    }
    if (src_x + w > sm_w) {
        w = sm_w - src_x;
        if (w <= 0)
            return;
    }
    if (src_y + h > sm_h) {
        h = sm_h - src_y;
        if (h <= 0)
            return;
    }

    uint8_t       *dst_base = dst->data + (size_t)dst_y * (size_t)dst->stride + (size_t)dst_x;
    const uint8_t *src_base = src->data + (size_t)src_y * (size_t)src->stride + (size_t)src_x;
    const uint8_t *msk_base = mask->data + (size_t)src_y * (size_t)mask->stride + (size_t)src_x;

    const size_t dst_row = (size_t)dst->stride;
    const size_t src_row = (size_t)src->stride;
    const size_t msk_row = (size_t)mask->stride;

    // Overlap-safe: if src and dst share backing memory, copy rows in a safe direction.
    const int same_storage = (dst->data == src->data);

    int y0 = 0, y1 = h, ystep = 1;
    if (same_storage && dst_base > src_base) {
        y0    = h - 1;
        y1    = -1;
        ystep = -1;
    }

    for (int y = y0; y != y1; y += ystep) {
        uint8_t       *d = dst_base + (size_t)y * dst_row;
        const uint8_t *s = src_base + (size_t)y * src_row;
        const uint8_t *m = msk_base + (size_t)y * msk_row;

        // Scan the row and copy opaque runs in chunks.
        int x = 0;
        while (x < w) {
            // Skip transparent
            while (x < w && m[x] != 0) {
                x += 1;
            }
            if (x >= w)
                break;

            // Find run length
            int run = x;
            while (run < w && m[run] == 0) {
                run += 1;
            }

            const size_t n = (size_t)(run - x);
            if (same_storage) {
                memmove(d + (size_t)x, s + (size_t)x, n);
            } else {
                memcpy(d + (size_t)x, s + (size_t)x, n);
            }

            x = run;
        }
    }
}

void ega_buffer_blit_sprite(ega_buffer_t *dst, const ega_buffer_t *src, const ega_buffer_t *mask,
                            int dst_x, int dst_y) {
    ega_buffer_blit_masked(dst, src, mask, dst_x, dst_y, 0, 0, src->w, src->h);
}

void ega_buffer_blit_colour(ega_buffer_t *dst, const ega_buffer_t *mask, uint8_t colour, int dst_x,
                            int dst_y) {
    assert(dst);
    assert(mask);

    int w     = (int)mask->w;
    int h     = (int)mask->h;
    int src_x = 0;
    int src_y = 0;

    const int dst_w = (int)dst->w;
    const int dst_h = (int)dst->h;

    // Clip against destination (left/top)
    if (dst_x < 0) {
        int d = -dst_x;
        if (d >= w)
            return;
        dst_x = 0;
        src_x += d;
        w -= d;
    }
    if (dst_y < 0) {
        int d = -dst_y;
        if (d >= h)
            return;
        dst_y = 0;
        src_y += d;
        h -= d;
    }

    // Clip against mask (right/bottom)
    if (src_x >= (int)mask->w || src_y >= (int)mask->h) {
        return;
    }
    if (src_x + w > (int)mask->w) {
        w = (int)mask->w - src_x;
        if (w <= 0)
            return;
    }
    if (src_y + h > (int)mask->h) {
        h = (int)mask->h - src_y;
        if (h <= 0)
            return;
    }

    // Clip against destination (right/bottom)
    if (dst_x >= dst_w || dst_y >= dst_h) {
        return;
    }
    if (dst_x + w > dst_w) {
        w = dst_w - dst_x;
        if (w <= 0)
            return;
    }
    if (dst_y + h > dst_h) {
        h = dst_h - dst_y;
        if (h <= 0)
            return;
    }

    uint8_t       *dst_base = dst->data + (size_t)dst_y * (size_t)dst->stride + (size_t)dst_x;
    const uint8_t *msk_base = mask->data + (size_t)src_y * (size_t)mask->stride + (size_t)src_x;

    const size_t dst_row = (size_t)dst->stride;
    const size_t msk_row = (size_t)mask->stride;

    for (int y = 0; y < h; y += 1) {
        uint8_t       *d = dst_base + (size_t)y * dst_row;
        const uint8_t *m = msk_base + (size_t)y * msk_row;
        for (int x = 0; x < w; x += 1) {
            if (m[x] != 0) {
                d[x] = colour;
            }
        }
    }
}

void ega_render_buffer(uint32_t *dst, const uint8_t *src, const int w, const int h,
                       const uint8_t *pal) {
    for (int y = 0; y < h; y++) {
        int src_y = y * EGA_SCREEN_HEIGHT / h;
        for (int x = 0; x < w; x++) {
            int     src_x   = x * EGA_SCREEN_WIDTH / w;
            uint8_t ega_idx = src[src_y * EGA_SCREEN_WIDTH + src_x];
            if (pal != 0) {
                ega_idx = pal[ega_idx];
            }
            dst[y * w + x] = color_to_u32(EGA_PALETTE[ega_idx & 0x0F]);
        }
    }
}

void ega_decode_4_plane(uint8_t *dst, const uint8_t *src, uint16_t len, uint16_t offset) {
    uint16_t stride = len + offset;
    for (int i = 0; i < len; i++) {
        uint8_t b1 = *(src + i + stride * 0);
        uint8_t b2 = *(src + i + stride * 1);
        uint8_t b3 = *(src + i + stride * 2);
        uint8_t b4 = *(src + i + stride * 3);
        for (int m = 0; m < 8; m++) {
            uint8_t mask   = (uint8_t)(0x80 >> m);
            dst[8 * i + m] = (((b1 & mask) != 0) << 0) | (((b2 & mask) != 0) << 1) |
                             (((b3 & mask) != 0) << 2) | (((b4 & mask) != 0) << 3);
        }
    }
}

// void ega_decode_1_plane(uint8_t *dst, const uint8_t *src, uint16_t len)

void ega_decode_1bpp(uint8_t *dst, const uint8_t *src, uint16_t w, uint16_t h) {
    int bytes_per_row = (w + 7) >> 3;

    for (int y = 0; y < h; y++) {
        const uint8_t *row = src + y * bytes_per_row;
        uint8_t       *out = dst + y * w;

        int x = 0;
        for (int i = 0; i < bytes_per_row; i++) {
            uint8_t b = row[i];

            for (int m = 0; m < 8 && x < w; m++, x++) {
                uint8_t mask = (uint8_t)(0x80 >> m);
                out[x]       = (uint8_t)((b & mask) != 0);
            }
        }
    }
}
