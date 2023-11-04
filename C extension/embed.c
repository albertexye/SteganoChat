#include "embed.h"

#include "header.h"
#include "library.h"

#include <stdlib.h>

uint8_t **embed(void *precomputed, uint8_t **data) {
    struct Precomputed *p = (struct Precomputed *) precomputed;
    if (!p->successful) return NULL;

    uint8_t **result = (uint8_t **) calloc(p->valid_image_num, sizeof(uint8_t *));
    uint64_t index = 0;
    for (uint64_t i = 0; i < p->image_list->len; i++) {
        if (p->image_capacity_map[i] == 0) continue;
        struct Image *image = p->image_list->images + i;
        embed_image(image, data[index]);
        result[index] = image->pixels;
        index++;
    }

    return result;
}

void free_embedded(uint8_t **embedded) {
    free(embedded);
}

void embed_image(const struct Image *const image, const uint8_t *data) {
    uint64_t channel = image->channel;
    uint64_t width = image->width;
    for (uint64_t i = 0; i < image->coordinate_len; i++) {
        struct Coordinate c = image->coordinates[i];
        uint8_t *pixels = image->pixels + c.y * width * channel + c.x * channel;
        embed_square(pixels, width, channel, data);
        data += channel * 2;
    }
}

void embed_square(uint8_t *const pixels, const uint64_t width, const uint64_t channel, const uint8_t *const data) {
    uint64_t bit = 0;
    uint64_t byte = 0;
    for (uint64_t y = 0; y < SQUARE_SIZE; y++) {
        uint8_t *temp = pixels + width * channel;
        for (uint64_t x = 0; x < SQUARE_SIZE * channel; x++) {
            *temp = (*temp & 0b11111110) | ((data[byte] & (1 << bit)) >> bit);
            bit++;
            if (bit == 8) {
                bit = 0;
                byte++;
            }
            temp++;
        }
    }
}
