#ifndef C_EXTENSION_TEST_PRECOMPUTE_H
#define C_EXTENSION_TEST_PRECOMPUTE_H

#include <stdbool.h>
#include <inttypes.h>

uint8_t TESTING_IMAGE[] = {
    53, 90, 33, 86, 158, 84, 101, 44, 222, 104, 177, 141,
    11, 183, 3, 142, 209, 164, 34, 72, 45, 122, 136, 38,
    147, 44, 63, 80, 237, 127, 71, 44, 152, 97, 158, 118,
    180, 158, 96, 222, 7, 67, 48, 189, 159, 187, 192, 34
};

bool test_compare_squares();
bool test_get_square_amount();
bool test_calc_entropy();
bool test_count_pixel_value();
bool test_sort_squares();
bool test_locate_worst_image();
bool test_remove_useless_images();
bool test_compare_coordinates();
bool test_generate_squares();
bool test_sort_coordinates();
bool test_init_square_list();
bool test_init_image_list();
bool test_count_valid_image_num();

#endif //C_EXTENSION_TEST_PRECOMPUTE_H
