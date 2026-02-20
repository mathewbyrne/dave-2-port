#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#include "huff.cpp"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color24_t; // 24 bit Colour RGB

static const char *PPM_HEADER = "P6\n%d %d\n255\n";

static const color24_t EGA_PALETTE[16] = {
    { 0x00, 0x00, 0x00 }, // 00 Black
    { 0x00, 0x00, 0xAA }, // 01 Blue
    { 0x00, 0xAA, 0x00 }, // 02 Green
    { 0x00, 0xAA, 0xAA }, // 03 Cyan
    { 0xAA, 0x00, 0x00 }, // 04 Red
    { 0xAA, 0x00, 0xAA }, // 05 Magenta
    { 0xAA, 0x55, 0x00 }, // 06 Brown
    { 0xAA, 0xAA, 0xAA }, // 07 Light Gray
    { 0x55, 0x55, 0x55 }, // 08 Dark Gray
    { 0x55, 0x55, 0xFF }, // 09 Bright Blue
    { 0x55, 0xFF, 0x55 }, // 10 Bright Green
    { 0x55, 0xFF, 0xFF }, // 11 Bright Cyan
    { 0xFF, 0x55, 0x55 }, // 12 Bright Red
    { 0xFF, 0x55, 0xFF }, // 13 Bright Magenta
    { 0xFF, 0xFF, 0x55 }, // 14 Yellow
    { 0xFF, 0xFF, 0xFF }  // 15 White
};

/**
 * decode_ega_bitplanes takes an input buffer, and decodes 16 index bitplanes.
 * len should be the number of bytes per plane.  len * 8 bytes will be written
 * to dst.  offset can be > 0 in cases where the bitplanes are not sequential
 * in src.
 */
int decode_ega_bitplanes(uint8_t *dst, const uint8_t *src, uint16_t len, uint16_t offset) {
    uint16_t stride = len + offset;
    for (int i = 0; i < len; i++) {
        uint8_t b1 = *(src + i + stride * 0);
        uint8_t b2 = *(src + i + stride * 1);
        uint8_t b3 = *(src + i + stride * 2);
        uint8_t b4 = *(src + i + stride * 3);
        for (int m = 0; m < 8; m++) {
            uint8_t mask = (uint8_t)(0x80 >> m); // MSB first
            dst[8 * i + m] =
                (((b1 & mask) != 0) << 0) |
                (((b2 & mask) != 0) << 1) |
                (((b3 & mask) != 0) << 2) |
                (((b4 & mask) != 0) << 3);
        }
    }
    return 0;
}

/**
 * Same as decode_ega_bitplanes but only reads 1 bit rather than 4.
 */
int decode_ega_bitplanes_1bit(uint8_t *dst, const uint8_t *src, uint16_t len) {
    for (int i = 0; i < len; i++) {
        for (int m = 0; m < 8; m++) {
            uint8_t mask = (uint8_t)(0x80 >> m);
            dst[8 * i + m] = ((src[i] & mask) != 0);
        }
    }
    return 0;
}

typedef struct {
    unsigned char *data;
    size_t len;
} file_t;

/**
 * load_file takes a filename, and a destination pointer.  It will allocate
 * enough memory to store the file contents then load the contents and save the
 * address in *data.
 */
int load_file(const char *filename, file_t *out) {
    FILE *fp;
    if (fopen_s(&fp, filename, "rb") != 0) {
        printf("could not read file %s", filename);
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        printf("could not seek file");
        return 1;
    }
    
    out->len = ftell(fp);
    if (out->len < 0) {
        fclose(fp);
        printf("could not read file size");
        return 1;
    }

    rewind(fp);

    out->data = (unsigned char *)malloc(out->len);
    if (!out->data) {
        printf("could not allocate %zd bytes", out->len);
        return 1;
    }

    size_t read = fread(out->data, 1, out->len, fp);
    fclose(fp);

    if (read != out->len) {
        free(out->data);
        printf("read bytes differs from ftell");
        return 1;
    }

    return 0;
}

int load_entities() {
    file_t f;
    int file_err = load_file("dos/S_DAVE.DD2", &f);
    if (file_err != 0) {
        return file_err;
    }

    huff_src src = { f.data, f.len };
    size_t len;
    huff_err err = huff_len(src, &len);
    if (err != 0) {
        printf("huff error: %d", err);
        free(f.data);
        return 1;
    }
    printf("read %zd bytes, %zd bytes decoded\n", f.len, len);

    uint8_t *dst = (uint8_t *)malloc((size_t) len);
    if (!dst) {
        free(f.data);
        printf("could not allocate destination buffer");
        return 1;
    }
    huff_decode(src, dst, len);

    printf("decoded %zd bytes\n", len);

    printf("chunk size file: %d, chunk size actual: %d\n", *(uint16_t *)dst, (len - 2) / 5);
    for (int i = 0; i < 5; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%02X ", dst[i * 5056 + j + 2]);
        }
        printf("\n");
    }

    byte spr[512];
    uint16_t w, h, l, o = 8;
    w = 24;
    h = 32;
    l = (w / 8) * h;
    decode_ega_bitplanes(spr, dst + 2 + o, l, 5056 - l);

    FILE *fp;
    file_err = fopen_s(&fp, "sprite.ppm", "wb");
    if (file_err != 0) {
        printf("could not open output sprite file\n");
    }

    // Next we'll just confirm what we believe is the sprite mask.  The 5th
    // chunk in our sprite file should be a 1 bit mask.
    char header[64];
    sprintf(header, PPM_HEADER, w, h);
    fwrite(header, 1, strlen(header), fp);
    for (int i = 0; i < w * h; i++) {
        fwrite(EGA_PALETTE + spr[i], 3, 1, fp);
    }

    fclose(fp);

    file_err = fopen_s(&fp, "sprite_mask.ppm", "wb");
    if (file_err != 0) {
        printf("could not open output sprite file\n");
    }

    // attempt to get our mask then.
    decode_ega_bitplanes_1bit(spr, dst + 2 + o + (4 * 5056), l);

    sprintf(header, PPM_HEADER, w, h);
    fwrite(header, 1, strlen(header), fp);
    for (int i = 0; i < w * h; i++) {
        fwrite(EGA_PALETTE + spr[i], 3, 1, fp);
    }

    fclose(fp);

    free(f.data);
    free(dst);
    return 0;
}


typedef byte tile_sprite_t[256];

enum {
    TILES_WIDTH  = 13,
    TILES_HEIGHT = 66,
    TILES_COUNT  = (TILES_WIDTH * TILES_HEIGHT),

    TILE_W      = 16,
    TILE_H      = 16,
    TILE_PIXELS = TILE_W * TILE_H,

    TILES_STRIDE_PLANE = (TILE_PIXELS / 8),
    TILES_STRIDE       = (TILES_STRIDE_PLANE * 4),
};


int load_tiles() {
    file_t f;
    int file_err = load_file("dos/EGATILES.DD2", &f);
    if (file_err != 0) {
        return file_err;
    }

    tile_sprite_t spr[TILES_COUNT];
    memset(spr, 0, sizeof(spr));

    for (int t = 0; (t + 1) * TILES_STRIDE < f.len; t += 1) {
        decode_ega_bitplanes((uint8_t *)(spr + t), f.data + (t * TILES_STRIDE), TILE_W * TILE_H / 8, 0);
    }

    free(f.data);

    FILE *fp;
    file_err = fopen_s(&fp, "ega_tiles.ppm", "wb");
    if (file_err != 0) {
        printf("could not open output sprite file\n");
    }

    char header[64];
    sprintf(header, PPM_HEADER, TILES_WIDTH * TILE_W, TILES_HEIGHT * TILE_H);
    fwrite(header, 1, strlen(header), fp);
    for (int row = 0; row < TILES_HEIGHT; row++) {
        for (int scan = 0; scan < TILE_H; scan++) {
            for (int i = 0; i < TILES_WIDTH; i++) {
                for (int x = 0; x < TILE_W; x++) {
                    uint8_t idx = spr[row * TILES_WIDTH + i][scan * TILE_H + x];
                    fwrite(&EGA_PALETTE[idx].r, 1, 1, fp);
                    fwrite(&EGA_PALETTE[idx].g, 1, 1, fp);
                    fwrite(&EGA_PALETTE[idx].b, 1, 1, fp);
                }
            }
        }
    }

    fclose(fp);

    printf("loading tiles, %zd bytes, %zd sprites\n", f.len, f.len / 128);
    return 0;
}

int load_title() {
    file_t f;
    int file_err = load_file("dos/TITLE1.DD2", &f);
    if (file_err != 0) {
        return file_err;
    }
    printf("loaded title file %zd bytes", f.len);

    byte data[320 * 200];
    memset(data, 0, sizeof(data));
    // The first 8 bytes of the title screens are "PIC\x00\x28\x00\c8\00". That
    // seems to read as 40 x 200, which is likely the byte length of a scanline
    // for the image.  I'm just skipping them here for brevity.
    decode_ega_bitplanes((uint8_t *)data, f.data + 8, 320 * 200 / 8, 0);

    free(f.data);

    FILE *fp;
    file_err = fopen_s(&fp, "title1.ppm", "wb");
    if (file_err != 0) {
        printf("could not open output sprite file\n");
    }

    char header[64];
    sprintf(header, PPM_HEADER, 320, 200);
    fwrite(header, 1, strlen(header), fp);
    for (int i = 0; i < 320 * 200; i++) {
        fwrite(EGA_PALETTE + data[i], 3, 1, fp);
    }

    fclose(fp);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "tiles") == 0) {
        return load_tiles();
    }

    if (argc == 2 && strcmp(argv[1], "title") == 0) {
        return load_title();
    }


    load_entities();
}
