#include <stdio.h>
#include <stdlib.h>

#include "huff.cpp"

/**
 * decode_ega_bitplanes takes an input buffer, and decodes 16 index bitplanes.
 * Bitplanes are considered sequential as width * height * 4 bit sequences.
 * Destination will be written as a sequential sequence of decoded indexes.
 */
int decode_ega_bitplanes(uint8_t *dst, const uint8_t *src, uint16_t width, uint16_t height) {
    uint16_t plane_len_bytes = width * height / 8;
    for (int i = 0; i < plane_len_bytes; i++) {
        uint8_t b1 = *(src + i + plane_len_bytes * 0);
        uint8_t b2 = *(src + i + plane_len_bytes * 1);
        uint8_t b3 = *(src + i + plane_len_bytes * 2);
        uint8_t b4 = *(src + i + plane_len_bytes * 3);
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
 * load_file takes a filename, and a destination pointer.  It will allocate
 * enough memory to store the file contents then load the contents and save the
 * address in *data.
 */
int load_file(char *filename, uint8_t **data_ptr, size_t *size_ptr) {
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
    
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        printf("could not read file size");
        return 1;
    }

    rewind(fp);

    uint8_t *data = (uint8_t *)malloc((size_t) size);
    if (!data) {
        printf("could not allocate %ld bytes", size);
        return 1;
    }

    size_t read = fread(data, 1, (size_t) size, fp);
    fclose(fp);

    if (read != (size_t) size) {
        free(data);
        printf("read bytes differs from ftell");
        return 1;
    }

    *data_ptr = data;
    *size_ptr = size;
    return 0;
}

int load_entities() {
    uint8_t *data;
    size_t size;
    int file_err = load_file("dos/S_DAVE.DD2", &data, &size);
    if (file_err != 0) {
        return file_err;
    }

    huff_src src = { data, (size_t) size };
    size_t len;
    huff_err err = huff_len(src, &len);
    if (err != 0) {
        printf("huff error: %d", err);
        free(data);
        return 1;
    }
    printf("read %zd bytes, %zd bytes decoded\n", size, len);

    uint8_t *dst = (uint8_t *)malloc((size_t) len);
    if (!dst) {
        free(data);
        printf("could not allocate destination buffer");
        return 1;
    }
    huff_decode(src, dst, len);

    printf("decoded %zd bytes\n", len);


    free(data);
    return 0;
}

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color24; // 24 bit Colour RGB
typedef uint8_t IndexedSprite[256]; // 16x16 EGA Sprite (16 color)

static const Color24 EGA_PALETTE[16] = {
    { 0x00, 0x00, 0x00 }, // 0  Black
    { 0x00, 0x00, 0xAA }, // 1  Blue
    { 0x00, 0xAA, 0x00 }, // 2  Green
    { 0x00, 0xAA, 0xAA }, // 3  Cyan
    { 0xAA, 0x00, 0x00 }, // 4  Red
    { 0xAA, 0x00, 0xAA }, // 5  Magenta
    { 0xAA, 0x55, 0x00 }, // 6  Brown
    { 0xAA, 0xAA, 0xAA }, // 7  Light Gray
    { 0x55, 0x55, 0x55 }, // 8  Dark Gray
    { 0x55, 0x55, 0xFF }, // 9  Bright Blue
    { 0x55, 0xFF, 0x55 }, // 10 Bright Green
    { 0x55, 0xFF, 0xFF }, // 11 Bright Cyan
    { 0xFF, 0x55, 0x55 }, // 12 Bright Red
    { 0xFF, 0x55, 0xFF }, // 13 Bright Magenta
    { 0xFF, 0xFF, 0x55 }, // 14 Yellow
    { 0xFF, 0xFF, 0xFF }  // 15 White
};

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

static const char *PPM_HEADER = "P6\n%d %d\n255\n";

int load_tiles() {
    uint8_t *data;
    size_t size;
    int file_err = load_file("dos/EGATILES.DD2", &data, &size);
    if (file_err != 0) {
        return file_err;
    }

    IndexedSprite spr[TILES_COUNT];
    memset(spr, 0, sizeof(spr));

    for (int t = 0; (t + 1) * TILES_STRIDE < size; t += 1) {
        decode_ega_bitplanes((uint8_t *)(spr + t), data + (t * TILES_STRIDE), TILE_W, TILE_H);
    }

    FILE *fp;
    file_err = fopen_s(&fp, "output.ppm", "wb");
    if (file_err != 0) {
        printf("could not open output sprite file\n");
    }

    // char header[64];
    // sprintf(header, PPM_HEADER, 1 * 16, 1 * 16);
    // fwrite(header, 1, strlen(header), fp);
    //
    // for (int i = 0; i < 256; i++) {
    //     fwrite(EGA_PALETTE + spr[0][i], 1, 3, fp);
    // }

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

    printf("loading tiles, %zd bytes, %zd sprites\n", size, size / 128);
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "tiles") == 0) {
        return load_tiles();
    }

    load_entities();
}
