#include "library.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>


uint8_t *
embed(uint8_t *data, uint64_t data_len, uint8_t *pixels, uint64_t image_w, uint64_t image_h, uint64_t image_c) {
    Image image = {NULL, image_w, image_h, image_c};
    const uint64_t image_size = image_w * image_h * image_c;
    image.pixels = (uint8_t *) malloc(image_size);
    memcpy(image.pixels, pixels, image_size);
    Square *squares = get_squares(&image);
    if (!embed_data(&image, squares, data, data_len)) return NULL;
    free(squares);
    return image.pixels;
}


Data *extract(const uint8_t *pixels, uint64_t image_w, uint64_t image_h, uint64_t image_c) {
    Image image = {(uint8_t *) pixels, image_w, image_h, image_c};
    Square *squares = get_squares(&image);
    Data *data = extract_data(&image, squares);
    free(squares);
    return data;
}

uint8_t *get_data(const Data *data) {
    return data->data;
}

uint64_t get_len(const Data *data) {
    return data->len;
}


uint64_t calc_square_size(const Image *image) {
    return SQUARE_SIZE * SQUARE_SIZE * image->c / 8;
}


uint64_t calc_image_capacity(const Image *image) {
    const uint64_t square_w = image->w / SQUARE_SIZE;
    const uint64_t square_h = image->h / SQUARE_SIZE;
    const uint64_t square_size = calc_square_size(image);
    return (square_w * square_h - 1) * square_size;
}


void calc_entropy(const Image *image, Square *square) {
    uint8_t map[256];
    for (uint64_t c = 0; c < image->c; c++) {
        memset(map, 0, sizeof(map));
        for (uint64_t i = 0; i < SQUARE_SIZE; i++) {
            for (uint64_t j = 0; j < SQUARE_SIZE; j++) {
                uint8_t pixel = image->pixels[c + ((square->y + i) * image->w * image->c + (square->x + j) * image->c)];
                map[pixel]++;
            }
        }
    }
    square->entropy = 0;
    for (uint64_t i = 0; i < 256; i++) {
        if (map[i] == 0) continue;
        square->entropy += (double) map[i] * log2(map[i]);
    }
}

int compare_squares(const void *a, const void *b) {
    const Square *square_a = (const Square *) a;
    const Square *square_b = (const Square *) b;
    if (square_a->entropy > square_b->entropy) return -1;
    else if (square_a->entropy < square_b->entropy) return 1;
    else return 0;
}

Square *get_squares(const Image *image) {
    const uint64_t square_w = image->w / SQUARE_SIZE;
    const uint64_t square_h = image->h / SQUARE_SIZE;
    const uint64_t size = square_w * square_h;
    Square *squares = (Square *) malloc(sizeof(Square) * size);
    for (uint64_t i = 0; i < square_h; i++) {
        for (uint64_t j = 0; j < square_w; j++) {
            Square *square = squares + i * square_w + j;
            square->y = i * SQUARE_SIZE;
            square->x = j * SQUARE_SIZE;
            calc_entropy(image, square);
        }
    }
    qsort(squares, size, sizeof(Square), compare_squares);
    return squares;
}

void embed_square(const Image *image, const Square *square, const uint8_t *data) {
    uint64_t index = 0, bit = 0;
    for (uint64_t i = 0; i < SQUARE_SIZE; i++) {
        for (uint64_t j = 0; j < SQUARE_SIZE; j++) {
            for (uint64_t c = 0; c < image->c; c++) {
                uint64_t offset = square->y * image->w * image->c + square->x * image->c + c;
                image->pixels[offset] |= ((data[index] & (1 << bit)) >> bit);
                if (++bit == 8) {
                    bit = 0;
                    index++;
                }
            }
        }
    }
}

bool embed_data(const Image *image, const Square *squares, const uint8_t *data, uint64_t data_len) {
    const uint64_t capacity = calc_image_capacity(image);
    if (capacity < data_len) {
        return false;
    }
    const uint64_t square_size = calc_square_size(image);
    const uint64_t padding_size = data_len % square_size;
    const uint64_t square_n = data_len / square_size;

    uint8_t prefix[square_size];
    memset(prefix, 0, square_size);
    *((uint64_t *) prefix) = data_len;
    embed_square(image, squares, prefix);
    squares++;

    for (uint64_t i = 0; i < square_n; i++) {
        embed_square(image, squares + i, data + i * square_size);
    }

    if (padding_size) {
        uint8_t padding[square_size];
        memset(padding, 0, square_size);
        memcpy(padding, data + data_len - padding_size, padding_size);
        embed_square(image, squares + square_n, padding);
    }

    return true;
}

void extract_square(const Image *image, const Square *square, uint8_t *data) {
    uint64_t index = 0, bit = 0;
    for (uint64_t i = 0; i < SQUARE_SIZE; i++) {
        for (uint64_t j = 0; j < SQUARE_SIZE; j++) {
            for (uint64_t c = 0; c < image->c; c++) {
                uint8_t *pixel = image->pixels + (square->y * image->w * image->c + square->x * image->c + c);
                data[index] |= (*pixel & 1) << bit;
                if (++bit == 8) {
                    bit = 0;
                    index++;
                }
            }
        }
    }
}

uint64_t extract_length(const Image *image, const Square *square) {
    const uint64_t square_size = calc_square_size(image);
    uint8_t prefix[square_size];
    extract_square(image, square, prefix);
    const uint64_t data_len = *((uint64_t *) prefix);
    return data_len;
}

Data *extract_data(const Image *image, const Square *squares) {
    const uint64_t square_size = calc_square_size(image);
    const uint64_t data_len = extract_length(image, squares);
    const uint64_t capacity = calc_image_capacity(image);
    if (capacity < data_len || data_len == 0) return NULL;
    squares++;

    const uint64_t padding_size = data_len % square_size;
    const uint64_t square_n = data_len / square_size;

    uint8_t *data = (uint8_t *) malloc(data_len);
    memset(data, 0, data_len);
    for (uint64_t i = 0; i < square_n; i++) {
        extract_square(image, squares + i, data + i * square_size);
    }

    if (padding_size) {
        uint8_t padding[square_size];
        memset(padding, 0, square_size);
        extract_square(image, squares + square_n, padding);
        memcpy(data + data_len - padding_size, padding, padding_size);
    }

    Data *result = (Data *) malloc(sizeof(Data));
    result->data = data;
    result->len = data_len;

    return result;
}
