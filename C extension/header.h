#ifndef C_EXTENSION_HEADER_H
#define C_EXTENSION_HEADER_H

#include <inttypes.h>
#include <stdbool.h>

static const uint64_t SQUARE_SIZE = 4;

struct Coordinate {
    uint64_t x, y;
};

struct Image {
    uint8_t *pixels;
    uint64_t width, height;
    uint64_t channel;
    uint64_t coordinate_len;
    struct Coordinate *coordinates;
};

struct ImageList {
    struct Image *images;
    uint64_t len;
};

struct Precomputed {
    bool successful;
    uint64_t valid_image_num;
    uint64_t *image_capacity_map;
    struct ImageList *image_list;
};

#endif //C_EXTENSION_HEADER_H
