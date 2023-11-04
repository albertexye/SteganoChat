#ifndef C_EXTENSION_EMBED_H
#define C_EXTENSION_EMBED_H

#include <inttypes.h>

#include "header.h"

void embed_square(uint8_t *pixels, uint64_t width, uint64_t channel, const uint8_t *data); // TODO uint test

void embed_image(const struct Image *image, const uint8_t *data); // TODO uint test

#endif //C_EXTENSION_EMBED_H
