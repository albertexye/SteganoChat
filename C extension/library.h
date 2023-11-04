#ifndef C_EXTENSION_LIBRARY_H
#define C_EXTENSION_LIBRARY_H

#include <inttypes.h>
#include <stdbool.h>

uint8_t *embed(uint8_t *data, uint64_t data_len, uint8_t *pixel, uint64_t image_w, uint64_t image_h, uint64_t image_c);

uint8_t *extract(const uint8_t *pixels, uint64_t image_w, uint64_t image_h, uint64_t image_c);

const uint64_t SQUARE_SIZE = 8;

typedef struct Image {
    uint8_t *pixels;
    uint64_t w, h, c;
} Image;

typedef struct Square {
    uint64_t x, y;
    double entropy;
} Square;

uint64_t calc_square_size(const Image *image);

uint64_t calc_image_capacity(const Image *image);

void calc_entropy(const Image *image, Square *square);

int compare_squares(const void *a, const void *b);

Square *get_squares(const Image *image);

void embed_square(const Image *image, const Square *square, const uint8_t *data);

bool embed_data(const Image *image, const Square *squares, const uint8_t *data, uint64_t data_len);

void extract_square(const Image *image, const Square *square, uint8_t *data);

uint64_t extract_length(const Image *image, const Square *squares);

uint8_t *extract_data(const Image *image, const Square *squares);

#endif //C_EXTENSION_LIBRARY_H
