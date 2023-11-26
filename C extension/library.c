#include "library.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

const uint64_t SquareSize = 16;

const uint64_t
        OK = 0,
        AllocationFailure = 1,
        OversizedData = 2,
        BadDataPiecesLen = 3,
        BadPrecomputed = 4,
        InvalidLen = 5;

uint64_t copy_image(Image *const image) {
    if (image->copied) return OK;
    const uint64_t size = image->w * image->h * image->c;
    uint8_t *const pixels = (uint8_t *) calloc(size, sizeof(uint8_t));
    if (pixels == NULL) return AllocationFailure;
    memcpy(pixels, image->pixels, size);
    image->pixels = pixels;
    image->copied = true;
    return OK;
}

uint64_t copy_imageList(ImageList *const imageList) {
    Image *const images = (Image *) calloc(imageList->len, sizeof(Image));
    if (images == NULL) return AllocationFailure;
    memcpy(images, imageList->images, imageList->len * sizeof(Image));
    for (uint64_t i = 0; i < imageList->len; ++i) {
        const uint64_t code = copy_image(images + i);
        if (code != OK) {
            free_imageList(imageList);
            return code;
        }
    }
    imageList->images = images;
    return OK;
}

void calc_entropy(const Image *const image, Square *const square) {
    double entropy = 0.;
    const uint64_t channel = image->c, width = image->w;
    const uint64_t real_width = width * channel;
    uint8_t map[128];
    const uint8_t *start = image->pixels + (square->y * width + square->x) * channel;
    for (uint64_t c = 0; c < channel; ++c, ++start) {
        memset(map, 0, 128);
        const uint8_t *y_start = start;
        for (uint64_t y = 0; y < SquareSize; ++y, y_start += real_width) {
            const uint8_t *x_start = y_start;
            for (uint64_t x = 0; x < SquareSize; ++x, x_start += channel) {
                ++(map[(*x_start) >> 1]);
            }
        }
        for (uint64_t i = 0; i < 128; ++i) {
            if (map[i] == 0) continue;
            const double p = (double) map[i] / (double) (SquareSize * SquareSize);
            entropy += -p * log2(p);
        }
    }
    square->entropy = entropy / (double) channel;
}

int compare_squares(const void *const a, const void *const b) {
    if (((const Square *) a)->entropy > ((const Square *) b)->entropy) return -1;
    else if (((const Square *) a)->entropy < ((const Square *) b)->entropy) return 1;
    return 0;
}

uint64_t generate_squares(Image *const image, const uint64_t reserved) {
    const uint64_t square_w = image->w / SquareSize;
    const uint64_t square_h = image->h / SquareSize;
    const uint64_t size = square_w * square_h;
    if (size <= (uint64_t) ceil((double) reserved / (double) ((SquareSize * SquareSize >> 3) * image->c))) return OK;

    Square *const squares = (Square *) calloc(size, sizeof(Square));
    if (squares == NULL) return AllocationFailure;

    Square *square = squares;
    for (uint64_t i = 0; i < square_h; ++i) {
        for (uint64_t j = 0; j < square_w; ++j, ++square) {
            square->y = i * SquareSize;
            square->x = j * SquareSize;
            calc_entropy(image, square);
        }
    }
    qsort(squares, size, sizeof(Square), compare_squares);

    image->squareList = (SquareList) {
            squares, size
    };

    return OK;
}

uint64_t init_imageList(ImageList *const imageList, const uint64_t reserved) {
    uint64_t code;

    code = copy_imageList(imageList);
    if (code != OK) return code;

    for (uint64_t i = 0; i < imageList->len; ++i) {
        code = generate_squares(imageList->images + i, reserved);
        if (code != OK) {
            free_imageList(imageList);
            return code;
        }
    }

    return OK;
}

uint64_t count_images(ImageList *const imageList, const uint64_t dataLen, const uint64_t reserved) {
    const uint64_t imageLen = imageList->len;
    uint64_t size = 0;

    uint64_t *const squareIndex = (uint64_t *) calloc(imageLen, sizeof(uint64_t));
    if (squareIndex == NULL) return AllocationFailure;
    for (uint64_t i = 0; i < imageLen; ++i) {
        Image *const image = imageList->images + i;
        const uint64_t squareLen = SquareSize * SquareSize * image->c / 8;
        const uint64_t count = (uint64_t) ceil((double) reserved / (double) squareLen);
        squareIndex[i] = count + 1;
        image->usage = count;
        size += squareLen * count - reserved;
    }

    const uint64_t squareLen = SquareSize * SquareSize / 8;
    bool enough = (size >= dataLen);
    for (; !enough; enough = (size >= dataLen)) {
        double maxEntropy = 0.;
        uint64_t maxIndex = 0;
        Image *maxImage = NULL;
        for (uint64_t i = 0; i < imageLen; ++i) {
            Image *const image = imageList->images + i;
            const Square *const squares = image->squareList.squares;
            if (squares == NULL) continue;
            if (squareIndex[i] == image->squareList.len) continue;
            const double entropy = squares[squareIndex[i]].entropy;
            if (entropy > maxEntropy) {
                maxEntropy = entropy;
                maxIndex = i;
                maxImage = image;
            }
        }
        if (maxImage == NULL) break;
        ++(squareIndex[maxIndex]);
        ++(maxImage->usage);
        size += squareLen * maxImage->c;
    }
    free(squareIndex);
    if (!enough) return OversizedData;

    return OK;
}

void prune_images(ImageList *const imageList) {
    const uint64_t imageLen = imageList->len;
    for (uint64_t i = 0; i < imageLen; ++i) {
        Image *const image = imageList->images + i;
        if (image->usage + 1 < image->squareList.len) {
            Square *squares = (Square *) calloc(image->usage + 1, sizeof(Square));
            memcpy(squares, image->squareList.squares, (image->usage + 1) * sizeof(Square));
            free(image->squareList.squares);
            image->squareList.len = image->usage + i;
            image->squareList.squares = squares;
        }
    }
}

Precomputed precompute(ImageList imageList, const uint64_t dataLen, const uint64_t reserved) {
    uint64_t code;

    code = init_imageList(&imageList, reserved);
    if (code != OK) {
        free_imageList(&imageList);
        return (Precomputed) {{NULL, 0}, code};
    }

    code = count_images(&imageList, dataLen, reserved);
    if (code != OK) {
        free_imageList(&imageList);
        return (Precomputed) {{NULL, 0}, code};
    }

    prune_images(&imageList);

    return (Precomputed) {imageList, OK};
}

void free_image(Image *const image) {
    if (!image->copied) return;
    free(image->pixels);
    free(image->squareList.squares);
    *image = (Image) {0, 0, 0, NULL, {NULL, 0}, 0, false};
}

void free_imageList(ImageList *const imageList) {
    if (imageList->images == NULL) return;
    const uint64_t len = imageList->len;
    for (uint64_t i = 0; i < len; ++i) free_image(imageList->images + i);
    free(imageList->images);
    imageList->images = NULL;
}

void free_precomputed(Precomputed precomputed) {
    free_imageList(&precomputed.imageList);
}

void embed_len(Image *const image, const Square *const square, const uint64_t len) {
    const uint64_t channel = image->c;
    const uint64_t realWidth = image->w * channel;;
    uint8_t *const data = (uint8_t *) &len;
    uint64_t index = 0, bit = 0;
    uint8_t *yStart = image->pixels + (square->y * image->w + square->x) * channel;
    for (uint64_t y = 0; y < SquareSize; ++y, yStart += realWidth) {
        uint8_t *xStart = yStart;
        for (uint64_t x = 0; x < SquareSize; ++x) {
            for (uint64_t c = 0; c < channel; ++c, ++xStart) {
                *xStart &= (uint8_t) 0b11111110;
                *xStart |= (uint8_t) ((data[index] & (1 << bit)) >> bit);
                if (++bit == 8) {
                    bit = 0;
                    if (++index == sizeof(uint64_t)) return;
                }
            }
        }
    }
}

void embed_square(Image *const image, const Square *const square, const uint8_t *const data) {
    const uint64_t channel = image->c;
    const uint64_t realWidth = image->w * channel;
    uint64_t index = 0, bit = 0;
    uint8_t *yStart = image->pixels + (square->y * image->w + square->x) * channel;
    for (uint64_t y = 0; y < SquareSize; ++y, yStart += realWidth) {
        uint8_t *xStart = yStart;
        for (uint64_t x = 0; x < SquareSize; ++x) {
            for (uint64_t c = 0; c < channel; ++c, ++xStart) {
                *xStart &= (uint8_t) 0b11111110;
                *xStart |= (uint8_t) ((data[index] & (1 << bit)) >> bit);
                if (++bit == 8) {
                    bit = 0;
                    ++index;
                }
            }
        }
    }
}

void embed_image(Image *const image, const Data *const data) {
    const uint64_t squareLen = SquareSize * SquareSize * image->c / 8;
    Square *const squares = image->squareList.squares;
    embed_len(image, squares, data->len);
    const uint8_t *dataPtr = data->data;
    const uint64_t usage = image->usage;
    for (uint64_t i = 0; i < usage; ++i) {
        embed_square(image, squares + i + 1, dataPtr);
        dataPtr += squareLen;
    }
}

uint64_t padding(const Image *const image, Data *const data) {
    if (data->len == 0 || data->data == NULL || data->padded) return OK;
    const uint64_t squareLen = SquareSize * SquareSize * image->c / 8;
    const uint64_t paddedLen = squareLen * (data->len / squareLen + (data->len % squareLen != 0));
    uint8_t *const padded = (uint8_t *) calloc(paddedLen, sizeof(uint8_t));
    if (padded == NULL) return AllocationFailure;
    memcpy(padded, data->data, data->len);
    data->data = padded;
    data->padded = true;
    return OK;
}

void free_data(Data *const data) {
    if (data->padded) {
        free(data->data);
        *data = (Data) {NULL, 0, false};
    }
}

void free_dataPieces(DataPieces *const dataPieces) {
    const uint64_t len = dataPieces->len;
    for (uint64_t i = 0; i < len; ++i) free_data(dataPieces->pieces + i);
    *dataPieces = (DataPieces) {NULL, 0};
}

Embedded embed(Precomputed precomputed, DataPieces dataPieces) {
    if (precomputed.code != OK) return (Embedded) {{NULL, 0}, BadPrecomputed};
    const uint64_t len = dataPieces.len;
    uint64_t code;
    if (precomputed.imageList.len != len) return (Embedded) {{NULL, 0}, BadDataPiecesLen};
    for (uint64_t i = 0; i < len; ++i) {
        Image *const image = precomputed.imageList.images + i;
        Data *const piece = dataPieces.pieces + i;
        code = padding(image, piece);
        if (code != OK) {
            free_dataPieces(&dataPieces);
            return (Embedded) {{NULL, 0}, AllocationFailure};
        }
        embed_image(image, piece);
    }
    return precomputed;
}

uint64_t extract_len(const Image *const image, const Square *const square) {
    uint64_t len = 0;
    const uint64_t channel = image->c;
    const uint64_t realWidth = image->w * channel;;
    uint8_t *const data = (uint8_t *) &len;
    uint64_t index = 0, bit = 0;
    uint8_t *yStart = image->pixels + (square->y * image->w + square->x) * channel;
    for (uint64_t y = 0; y < SquareSize; ++y, yStart += realWidth) {
        uint8_t *xStart = yStart;
        for (uint64_t x = 0; x < SquareSize; ++x) {
            for (uint64_t c = 0; c < channel; ++c, ++xStart) {
                data[index] |= (*xStart & 1) << bit;
                if (++bit == 8) {
                    bit = 0;
                    if (++index == sizeof(uint64_t)) return len;
                }
            }
        }
    }
    return len;
}

void extract_data(const Image *const image, const Square *const square, uint8_t *const data) {
    const uint64_t realWidth = image->w * image->c;
    uint64_t index = 0, bit = 0;
    uint8_t *yStart = image->pixels + square->y * realWidth + square->x * image->c;
    for (uint64_t y = 0; y < SquareSize; ++y, yStart += realWidth) {
        uint8_t *xStart = yStart;
        for (uint64_t x = 0; x < SquareSize; ++x) {
            for (uint64_t c = 0; c < image->c; ++c, ++xStart) {
                data[index] |= (*xStart & 1) << bit;
                if (++bit == 8) {
                    bit = 0;
                    ++index;
                }
            }
        }
    }
}

Extracted extract(Image image, const uint64_t reserved) {
    uint64_t code;

    code = copy_image(&image);
    if (code != OK) return (Extracted) {{NULL, 0, false}, code};

    code = generate_squares(&image, reserved);
    if (code != OK) {
        free_image(&image);
        return (Extracted) {{NULL, 0, false}, code};
    }

    const uint64_t len = extract_len(&image, image.squareList.squares);
    const uint64_t squareLen = SquareSize * SquareSize * image.c / 8;
    const uint64_t squareNum = len / squareLen + (len % squareLen != 0);
    if (squareNum > image.squareList.len) {
        free_image(&image);
        return (Extracted) {{NULL, 0, false}, InvalidLen};
    }
    const uint64_t paddedLen = squareLen * squareNum;

    uint8_t *const padded = (uint8_t *) calloc(paddedLen, sizeof(uint8_t));
    if (padded == NULL) {
        free_image(&image);
        return (Extracted) {{NULL, 0, false}, AllocationFailure};
    }

    for (uint64_t i = 0; i < squareNum; ++i)
        extract_data(&image, image.squareList.squares + i + 1, padded + squareLen * i);

    return (Extracted) {{padded, len, true}, OK};
}

void free_extracted(Extracted extracted) {
    free_data(&extracted.data);
}
