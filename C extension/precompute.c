#include "precompute.h"
#include "library.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

void *
precompute(const uint8_t *const *images, const uint64_t *const image_widths, const uint64_t *const image_heights,
           const uint64_t *const image_channels, const uint64_t images_len, const uint64_t data_size,
           const uint64_t structure_size) {
    struct Precomputed *precomputed = (struct Precomputed *) malloc(sizeof(struct Precomputed));

    precomputed->image_capacity_map = (uint64_t *) calloc(images_len, sizeof(uint64_t));

    precomputed->image_list = (struct ImageList *) malloc(sizeof(struct ImageList));
    init_image_list(precomputed->image_list, images, image_widths, image_heights, image_channels, images_len);

    struct SquareList *square_list = (struct SquareList *) malloc(sizeof(struct SquareList));
    init_square_list(square_list, precomputed->image_list);

    precomputed->successful = prune_squares(precomputed, square_list, data_size, structure_size);

    free(square_list);

    return precomputed;
}

void
init_image_list(struct ImageList *image_list, const uint8_t *const *images, const uint64_t *const image_widths,
                const uint64_t *const image_heights, const uint64_t *const image_channels, const uint64_t images_len) {
    image_list->len = images_len;
    image_list->images = (struct Image *) calloc(images_len, sizeof(struct Image));

    for (uint64_t i = 0; i < images_len; i++) {
        uint64_t w = image_widths[i];
        uint64_t h = image_heights[i];
        uint64_t c = image_channels[i];
        uint64_t size = w * h * c;
        uint8_t *pixels = (uint8_t *) malloc(size);
        memcpy(pixels, images[i], size);
        image_list->images[i] = (struct Image) {
                pixels, w, h, c, 0, NULL
        };
    }
}

void init_square_list(struct SquareList *square_list, const struct ImageList *image_list) {
    uint64_t square_number_sum = 0;
    uint64_t square_numbers[image_list->len];
    for (uint64_t i = 0; i < image_list->len; i++) {
        uint64_t square_number = get_square_amount(image_list->images + i);
        square_numbers[i] = square_number;
        square_number_sum += square_number;
    }

    square_list->len = square_number_sum;
    square_list->squares = (struct Square *) calloc(square_number_sum, sizeof(struct Square));
    for (uint64_t i = 0, index = 0; i < image_list->len; i++) {
        generate_squares(image_list->images + i, i, square_list, index);
        index += square_numbers[i];
    }

    sort_squares(square_list);
}

void free_precomputed(void *precomputed) {
    struct Precomputed *p = (struct Precomputed *) precomputed;

    free(p->image_capacity_map);

    for (uint64_t i = 0; i < p->image_list->len; i++) {
        if (p->image_list->images[i].pixels != NULL)
            free(p->image_list->images[i].pixels);
        if (p->image_list->images[i].coordinates != NULL)
            free(p->image_list->images[i].coordinates);
    }
    free(p->image_list->images);
    free(p->image_list);

    free(p);
}

bool precomputed_successful(const void *const precomputed) {
    return ((struct Precomputed *) precomputed)->successful;
}

uint64_t *precomputed_image_capacity_map(const void *const precomputed) {
    return ((struct Precomputed *) precomputed)->image_capacity_map;
}

int compare_squares(const void *a, const void *b) {
    double e1 = ((struct Square *) a)->entropy;
    double e2 = ((struct Square *) b)->entropy;
    if (e1 > e2) {
        return -1;
    } else if (e1 < e2) {
        return 1;
    } else {
        return 0;
    }
}

void sort_squares(struct SquareList *square_list) {
    qsort(square_list->squares, square_list->len, sizeof(struct Square), compare_squares);
}

uint64_t get_square_amount(const struct Image *const image) {
    return (image->width / SQUARE_SIZE) * (image->height / SQUARE_SIZE);
}

double calc_entropy(const uint8_t *ptr, const uint64_t width, const uint64_t channel) {
    double entropy = 0;
    uint8_t map[128];
    for (uint64_t i = 0; i < channel; i++, ptr++) {
        count_pixel_value(ptr, width, channel, map);
        for (uint64_t j = 0; j < 128; j++) {
            double p = map[j];
            if (p == 0) continue;
			p /= SQUARE_SIZE * SQUARE_SIZE;
            entropy += p * log2(p);
        }
    }
    return -entropy / (double) channel;
}

void count_pixel_value(const uint8_t *const ptr, const uint64_t width, const uint64_t channel, uint8_t *const map) {
    memset(map, 0, 128);
    for (uint64_t y = 0; y < SQUARE_SIZE; y++) {
        const uint8_t *temp = ptr + y * width * channel;
        for (uint64_t x = 0; x < SQUARE_SIZE; x++) {
            map[(*temp) >> 1]++;
            temp += channel;
        }
    }
}

void
generate_squares(const struct Image *const image, const uint64_t image_index,
                 const struct SquareList *const square_list, const uint64_t square_index) {
    struct Square *square = square_list->squares + square_index;
    for (uint64_t y = 0; y + SQUARE_SIZE <= image->height; y += SQUARE_SIZE) {
        const uint8_t *pixel = image->pixels + y * image->width * image->channel;
        for (uint64_t x = 0; x + SQUARE_SIZE <= image->width; x += SQUARE_SIZE) {
            *square = (struct Square) {
                    image_index, calc_entropy(pixel, image->width, image->channel), {x, y}
            };
            pixel += SQUARE_SIZE * image->channel;
            square++;
        }
    }
}

bool
prune_squares(struct Precomputed *const precomputed, const struct SquareList *const square_list,
              const uint64_t data_size, const uint64_t structure_size) {
    uint64_t images_len = precomputed->image_list->len, squares_len = square_list->len;
    uint64_t *image_capacity_map = precomputed->image_capacity_map;

    bool needed_images[images_len];
    memset(needed_images, false, images_len);

    for (uint64_t remaining_image_num = images_len;; remaining_image_num--) {
        memset(image_capacity_map, 0, images_len);
        uint64_t size_needed = data_size + remaining_image_num * structure_size;
        uint64_t squares_used = match_squares(precomputed, square_list, needed_images, size_needed);
        if (squares_used == 0) return false;
        remove_useless_images(image_capacity_map, needed_images, squares_len);
        uint64_t worst_image_index = locate_worst_image(image_capacity_map, squares_len);
        if (image_capacity_map[worst_image_index] > structure_size) {
            track_back(precomputed, square_list, squares_used);
            return true;
        }
        needed_images[worst_image_index] = false;
    }
}

uint64_t
match_squares(const struct Precomputed *const precomputed, const struct SquareList *const square_list,
              const bool *const needed_images, const uint64_t size_needed) {
    uint64_t *image_capacity_map = precomputed->image_capacity_map;
    struct Square *squares = square_list->squares;
    struct Image *images = precomputed->image_list->images;

    memset(image_capacity_map, 0, precomputed->image_list->len);
    for (uint64_t i = 0, size_got = 0; i < square_list->len; i++) {
        uint64_t image_index = squares[i].image;
        if (needed_images[image_index]) {
            uint64_t square_size = (images + image_index)->channel * 2;
            size_got += square_size;
            image_capacity_map[image_index] += square_size;
            if (size_got >= size_needed) {
                return i + 1;
            }
        }
    }
    return 0;
}

uint64_t locate_worst_image(const uint64_t *const image_capacity_map, const uint64_t len) {
    uint64_t min = *image_capacity_map, index = 0;
    for (uint64_t i = 0; i < len; i++) {
        if (image_capacity_map[i] != 0) {
            min = image_capacity_map[i];
            index = i;
            break;
        }
    }
    for (uint64_t i = index; i < len; i++) {
        uint64_t capacity = image_capacity_map[i];
        if (capacity != 0 && capacity < min) {
            min = capacity;
            index = i;
        }
    }
    return index;
}

void
remove_useless_images(const uint64_t *const image_capacity_map, bool *const needed_images, const uint64_t len) {
    for (uint64_t i = 0; i < len; i++) {
        if (needed_images[i] && image_capacity_map[i] == 0) {
            needed_images[i] = false;
        }
    }
}

void
track_back(struct Precomputed *const precomputed, const struct SquareList *const square_list,
           const uint64_t squares_used) {
    alloc_coordinates(precomputed);
    set_coordinates(precomputed, square_list, squares_used);
    sort_coordinates(precomputed->image_list);
    free_useless_images(precomputed->image_list);
    count_valid_image_num(precomputed);
}

void alloc_coordinates(const struct Precomputed *const precomputed) {
    uint64_t *image_capacity_map = precomputed->image_capacity_map;
    struct Image *images = precomputed->image_list->images;

    for (uint64_t i = 0; i < precomputed->image_list->len; i++) {
        uint64_t image_capacity = image_capacity_map[i];
        if (image_capacity != 0) {
            struct Image *image = images + i;
            image->coordinate_len = image_capacity / image->channel / 2;
            image->coordinates = (struct Coordinate *) calloc(image->coordinate_len, sizeof(struct Coordinate));
        }
    }
}

void
set_coordinates(const struct Precomputed *const precomputed, const struct SquareList *const square_list,
                const uint64_t squares_used) {
    uint64_t *image_capacity_map = precomputed->image_capacity_map;
    struct Square *squares = square_list->squares;
    struct Image *images = precomputed->image_list->images;

    for (uint64_t i = 0; i < squares_used; i++) {
        struct Square *square = squares + i;
        if (image_capacity_map[square->image] != 0) {
            struct Image *image = images + square->image;
            image->coordinates[image->coordinate_len] = square->pos;
            image->coordinate_len++;
        }
    }
}

void sort_coordinates(const struct ImageList *const image_list) {
    for (uint64_t i = 0; i < image_list->len; i++) {
        struct Image *image = image_list->images + i;
        if (image->coordinates == NULL) continue;
        qsort(image_list->images->coordinates, image_list->images->coordinate_len, sizeof(struct Coordinate),
              compare_coordinates);
    }
}

int compare_coordinates(const void *a, const void *b) {
    struct Coordinate *c1 = (struct Coordinate *) a;
    struct Coordinate *c2 = (struct Coordinate *) b;
    if (c1->y > c2->y) return 1;
    else if (c1->y < c2->y) return -1;
    else if (c1->x > c2->x) return 1;
    else if (c1->x < c2->x) return -1;
    return 0;
}

void free_useless_images(const struct ImageList *const image_list) {
    for (uint64_t i = 0; i < image_list->len; i++) {
        struct Image *image = image_list->images + i;
        if (image->coordinates != NULL) continue;
        free(image->pixels);
        image->pixels = NULL;
    }
}

void count_valid_image_num(struct Precomputed *precomputed) {
    precomputed->valid_image_num = 0;
    for (uint64_t i = 0; i < precomputed->image_list->len; i++) {
        if (precomputed->image_capacity_map[i] != 0) {
            precomputed->valid_image_num++;
        }
    }
}
