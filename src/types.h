#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint8_t byte;

#define ASSERT(x) do { \
    if (!(x)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #x, __FILE__, __LINE__); \
        abort(); \
    } \
} while (0)

#endif

