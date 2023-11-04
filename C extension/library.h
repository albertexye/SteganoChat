#ifndef C_EXTENSION_LIBRARY_H
#define C_EXTENSION_LIBRARY_H

#include <inttypes.h>
#include <stdbool.h>

void *precompute(const uint8_t *const *images, const uint64_t *image_widths, const uint64_t *image_heights,
                 const uint64_t *image_channels, uint64_t images_len, uint64_t data_size, uint64_t structure_size); // TODO uint test

void free_precomputed(void *precomputed); // TODO uint test

bool precomputed_successful(const void *precomputed); // TODO uint test

uint64_t *precomputed_image_capacity_map(const void *precomputed); // TODO uint test

uint8_t **embed(void *precomputed, uint8_t **data); // TODO uint test

void free_embedded(uint8_t **embedded); // TODO uint test

#endif //C_EXTENSION_LIBRARY_H
