#ifndef C_EXTENSION_PRECOMPUTE_H
#define C_EXTENSION_PRECOMPUTE_H

#include <inttypes.h>
#include <stdbool.h>
#include "header.h"

struct Square {
    uint64_t image;
    double entropy;
    struct Coordinate pos;
};

struct SquareList {
    struct Square *squares;
    uint64_t len;
};

void init_image_list(struct ImageList *image_list, const uint8_t *const *images, const uint64_t *image_widths,
                     const uint64_t *image_heights, const uint64_t *image_channels, uint64_t images_len);

void init_square_list(struct SquareList *square_list, const struct ImageList *image_list);

int compare_squares(const void *a, const void *b);

void sort_squares(struct SquareList *square_list);

uint64_t get_square_amount(const struct Image *image);

double calc_entropy(const uint8_t *ptr, uint64_t width, uint64_t channel);

void count_pixel_value(const uint8_t *ptr, uint64_t width, uint64_t channel, uint8_t *map);

void generate_squares(const struct Image *image, uint64_t image_index, const struct SquareList *square_list,
                      uint64_t square_index);

bool prune_squares(struct Precomputed *precomputed, const struct SquareList *square_list, uint64_t data_size,
                   uint64_t structure_size); // TODO uint test

uint64_t
match_squares(const struct Precomputed *precomputed, const struct SquareList *square_list, const bool *needed_images,
              uint64_t size_needed); // TODO uint test

uint64_t locate_worst_image(const uint64_t *image_capacity_map, uint64_t len);

void remove_useless_images(const uint64_t *image_capacity_map, bool *const needed_images, uint64_t len);

void track_back(struct Precomputed *precomputed, const struct SquareList *square_list, uint64_t squares_used); // TODO uint test

void alloc_coordinates(const struct Precomputed *precomputed); // TODO uint test

void
set_coordinates(const struct Precomputed *precomputed, const struct SquareList *square_list, uint64_t squares_used); // TODO uint test

void sort_coordinates(const struct ImageList *image_list);

int compare_coordinates(const void *a, const void *b);

void free_useless_images(const struct ImageList *image_list); // TODO uint test

void count_valid_image_num(struct Precomputed *precomputed);

#endif //C_EXTENSION_PRECOMPUTE_H
