#include "test_precompute.h"
#include "testing.h"
#include "precompute.h"

#include <string.h>
#include <stdlib.h>

int main() {
    test("compare_squares", test_compare_squares);
    test("get_square_amount", test_get_square_amount);
    test("count_pixel_value", test_count_pixel_value);
    test("calc_entropy", test_calc_entropy);
    test("sort_squares", test_sort_squares);
    test("locate_worst_image", test_locate_worst_image);
    test("remove_useless_images", test_remove_useless_images);
    test("compare_coordinates", test_compare_coordinates);
    test("generate_squares", test_generate_squares);
    test("sort_coordinates", test_sort_coordinates);
    test("init_square_list", test_init_square_list);
    test("init_image_list", test_init_image_list);
    test("count_valid_image_num", test_count_valid_image_num);
    return 0;
}

bool test_compare_squares() {
    struct Square a = {}, b = {};

    TestEqual(compare_squares(&a, &b), 0);

    a.entropy++;
    TestEqual(compare_squares(&a, &b), -1);

    a.entropy--;
    b.entropy++;
    TestEqual(compare_squares(&a, &b), 1);

    return true;
}

bool test_get_square_amount() {
    struct Image image = {
            NULL, 5, 5, 3, 0, NULL
    };
    TestEqual(get_square_amount(&image), 1);
    return true;
}

bool test_calc_entropy() {
    ;
    double e = calc_entropy((const uint8_t *) TESTING_IMAGE, SQUARE_SIZE, 3);
    TestLog("entropy calculated: %.60g", e);
    TestEqual(e, 3.801879687409855268498404257115907967090606689453125);
    return true;
}

bool test_count_pixel_value() {
    uint8_t map[128];
    count_pixel_value(TESTING_IMAGE + 1, SQUARE_SIZE, 3, map);
    TestEqual(memcmp(map, (uint8_t[128]) {
            [3]=1, [22]=3, [36]=1, [45]=1, [68]=1, [79]=3, [88]=1, [91]=1, [94]=1, [96]=1, [104]=1, [118]=1,
    }, 128), 0);
    return true;
}

bool test_sort_squares() {
    struct SquareList squareList = {
            (struct Square[]) {
                    {0, 1.2, {0, 0}},
                    {0, 2.3, {0, 0}}
            }, 2
    };
    sort_squares(&squareList);
    TestEqual(squareList.squares[0].entropy, 2.3);
    TestEqual(squareList.squares[1].entropy, 1.2);
    return true;
}

bool test_locate_worst_image() {
    uint64_t image_capacity_map[] = {0, 3, 5, 2, 0, 4};
    uint64_t index = locate_worst_image(image_capacity_map, sizeof(image_capacity_map) / sizeof image_capacity_map[0]);
    TestEqual(index, 3);
    return true;
}

bool test_remove_useless_images() {
    uint64_t image_capacity_map[] = {0, 3, 5, 2, 0, 4};
    bool needed_images[] = {true, true, true, true, false, true};
    remove_useless_images(image_capacity_map, needed_images, sizeof(needed_images) / sizeof(needed_images[0]));
    TestEqual(needed_images[0], false);
    return true;
}

bool test_compare_coordinates() {
    struct Coordinate c1 = {
            3, 4
    }, c2 = {
            3, 4
    };

    TestEqual(compare_coordinates(&c1, &c2), 0);

    c1.y++;
    TestEqual(compare_coordinates(&c1, &c2), 1);

    c1.y--;
    c2.y++;
    TestEqual(compare_coordinates(&c1, &c2), -1);

    return true;
}

bool test_generate_squares() {
    struct Image image = {
            TESTING_IMAGE, SQUARE_SIZE, SQUARE_SIZE, 3, 0, NULL
    };
    struct Square squares[2];
    squares[0].entropy = 0;
    squares[0].image = 0;
    squares[0].pos = (struct Coordinate) {1, 1};
    squares[1] = squares[0];
    struct SquareList square_list = {
            squares, 2
    };

    generate_squares(&image, 1, &square_list, 1);

    TestEqual(squares[1].entropy, 3.801879687409855268498404257115907967090606689453125);
    TestEqual(squares[1].image, 1);
    TestEqual(squares[1].pos.x, 0);
    TestEqual(squares[1].pos.y, 0);

    return true;
}

bool test_sort_coordinates() {
    struct Coordinate coordinates[] = {
            {9,  8},
            {0,  0},
            {10, 8},
            {8,  12}
    };
    struct Image images[] = {
            {NULL, 0, 0, 0, sizeof(coordinates) / sizeof(coordinates[0]), coordinates}
    };
    struct ImageList image_list = {
            images, 1
    };

    sort_coordinates(&image_list);

    TestEqual(coordinates[0].x, 0)
    TestEqual(coordinates[0].y, 0)
    TestEqual(coordinates[1].x, 9)
    TestEqual(coordinates[1].y, 8)
    TestEqual(coordinates[2].x, 10)
    TestEqual(coordinates[2].y, 8)
    TestEqual(coordinates[3].x, 8)
    TestEqual(coordinates[3].y, 12)

    return true;
}

bool test_init_square_list() {
    struct Image image = {
            TESTING_IMAGE, SQUARE_SIZE, SQUARE_SIZE, 3, 0, NULL
    };
    struct ImageList image_list = {
            &image, 1
    };
    struct SquareList square_list = {
            NULL, 0
    };

    init_square_list(&square_list, &image_list);

    TestEqual(square_list.len, 1);
    TestEqual(square_list.squares[0].entropy, 3.801879687409855268498404257115907967090606689453125);
    TestEqual(square_list.squares[0].image, 0);
    TestEqual(square_list.squares[0].pos.x, 0);
    TestEqual(square_list.squares[0].pos.y, 0);

    free(square_list.squares);

    return true;
}

bool test_init_image_list() {
    struct ImageList image_list;
    uint64_t width = SQUARE_SIZE;
    uint64_t height = SQUARE_SIZE;
    uint64_t channel = 3;
    const uint8_t *pixels = TESTING_IMAGE;
    init_image_list(&image_list, &pixels, &width, &height, &channel, 1);

    TestEqual(image_list.images[0].width, SQUARE_SIZE);
    TestEqual(image_list.images[0].height, SQUARE_SIZE);
    TestEqual(image_list.images[0].channel, 3);
    TestEqual(memcmp(image_list.images[0].pixels, TESTING_IMAGE, 48), 0);
    TestEqual(image_list.images[0].coordinate_len, 0);
    TestEqual(image_list.images[0].coordinates, NULL);

    free(image_list.images[0].pixels);
    free(image_list.images);
    return true;
}

bool test_count_valid_image_num() {
    struct ImageList image_list = {
            NULL, 7
    };
    uint64_t map[] = {0, 1, 1, 0, 3, 2, 0};
    struct Precomputed precomputed = {
            true, 0, map, &image_list
    };
    count_valid_image_num(&precomputed);
    TestEqual(precomputed.valid_image_num, 4);
    return true;
}
