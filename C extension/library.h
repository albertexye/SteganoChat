#ifndef STEGANO_H
#define STEGANO_H

#include <inttypes.h>
#include <stdbool.h>

typedef struct Square {
    uint64_t x, y;
    double entropy;
} Square;

typedef struct SquareList {
    Square *squares;
    uint64_t len;
} SquareList;

typedef struct Image {
    uint64_t w, h, c;
    uint8_t *pixels;
    SquareList squareList;
    uint64_t usage;
    bool copied;
} Image;

typedef struct ImageList {
    Image *images;
    uint64_t len;
} ImageList;

typedef struct Data {
    uint8_t *data;
    uint64_t len;
    bool padded;
} Data;

typedef struct DataPieces {
    Data *pieces;
    uint64_t len;
} DataPieces;

typedef struct Precomputed {
    ImageList imageList;
    uint64_t code;
} Precomputed;

typedef struct Extracted {
    Data data;
    uint64_t code;
} Extracted;

typedef Precomputed Embedded;

Precomputed precompute(ImageList imageList, uint64_t dataLen, uint64_t reserved);

void free_precomputed(Precomputed precomputed);

Embedded embed(Precomputed precomputed, DataPieces dataPieces);

Extracted extract(Image image, uint64_t reserved);

void free_extracted(Extracted extracted);

extern const uint64_t SquareSize;

extern const uint64_t OK, AllocationFailure, OversizedData, BadDataPiecesLen, BadPrecomputed, InvalidLen;


uint64_t copy_imageList(ImageList *imageList);

void calc_entropy(const Image *image, Square *square);

int compare_squares(const void *a, const void *b);

uint64_t generate_squares(Image *image, uint64_t reservedSquareNum);

uint64_t init_imageList(ImageList *imageList, uint64_t reserved);

uint64_t count_images(ImageList *imageList, uint64_t dataLen, uint64_t reservedSquareNum);

void prune_images(ImageList *imageList);

void free_image(Image *image);

void free_imageList(ImageList *imageList);

void embed_len(Image *image, const Square *square, uint64_t len);

void embed_square(Image *image, const Square *square, const uint8_t *data);

void embed_image(Image *image, const Data *data);

uint64_t padding(const Image *image, Data *data);

void free_data(Data *data);

void free_dataPieces(DataPieces *dataPieces);

uint64_t extract_len(const Image *image, const Square *square);

void extract_data(const Image *image, const Square *square, uint8_t *data);

#endif