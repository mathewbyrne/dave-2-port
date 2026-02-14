#include <stdio.h>
#include <stdlib.h>

#include "huff.cpp"

int main() {
    FILE *fp;
    if (fopen_s(&fp, "dos/S_DAVE.DD2", "rb") != 0) {
        printf("could not read file S_DAVE.DD2");
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


    huff_src src = { data, (size_t) size };
    size_t len;
    huff_err err = huff_len(src, &len);
    if (err != 0) {
        printf("huff error: %d", err);
        free(data);
        return 1;
    }
    printf("read %ld bytes, %zd bytes decoded\n", size, len);

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

