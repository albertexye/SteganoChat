#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint64_t IMAGE_LEN = 5;

const uint64_t
        IMAGE_W[] = {256, 777, 129, 356, 852},
        IMAGE_H[] = {212, 345, 844, 333, 421},
        IMAGE_C[] = {1, 3, 2, 4, 3};

void randFill(uint8_t *data, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) data[i] = (uint8_t) rand();
}

Image createRandomImage(uint64_t w, uint64_t h, uint64_t c) {
    const uint64_t size = w * h * c;
    uint8_t *const pixels = (uint8_t *) calloc(size, sizeof(uint8_t));
    randFill(pixels, size);
    return (Image) {w, h, c, pixels, {NULL, 0}, 0, false};
}

ImageList createRandomImageList(void) {
    Image *images = (Image *) calloc(IMAGE_LEN, sizeof(Image));
    for (uint64_t i = 0; i < IMAGE_LEN; ++i) {
        const uint64_t w = IMAGE_W[i];
        const uint64_t h = IMAGE_H[i];
        const uint64_t c = IMAGE_C[i];
        images[i] = createRandomImage(w, h, c);
    }
    ImageList imageList = {images, IMAGE_LEN};
    return imageList;
}

Data randData(uint64_t len) {
    Data data = {
            (uint8_t *) calloc(len, sizeof(uint8_t)),
            len, false
    };
    randFill(data.data, len);
    return data;
}

DataPieces splitData(Precomputed precomputed, Data data, uint64_t reserved) {
    Data *pieces = (Data *) calloc(precomputed.imageList.len, sizeof(Data));
    uint64_t dataIndex = 0;
    for (uint64_t i = 0; i < precomputed.imageList.len; ++i) {
        Data *const piece = pieces + i;
        const Image *const image = precomputed.imageList.images + i;

        piece->len = SquareSize * SquareSize * image->c / 8 * image->usage;
        if (dataIndex + piece->len - reserved > data.len) {
            piece->len = data.len - dataIndex + reserved;
        }
        piece->data = (uint8_t *) calloc(piece->len, sizeof(uint8_t));
        randFill(piece->data, reserved);
        memcpy(piece->data + reserved, data.data + dataIndex, piece->len - reserved);
        piece->padded = false;
        dataIndex += piece->len - reserved;
    }
    DataPieces dataPieces = {pieces, precomputed.imageList.len};
    return dataPieces;
}

DataPieces extractImageList(ImageList imageList, uint64_t reserved) {
    DataPieces dataPieces = {(Data *) calloc(imageList.len, sizeof(Data)), imageList.len};

    for (uint64_t i = 0; i < imageList.len; ++i) {
        Extracted extracted = extract(imageList.images[i], reserved);
        if (extracted.code != OK) {
            free_dataPieces(&dataPieces);
            return dataPieces;
        }
        dataPieces.pieces[i] = extracted.data;
    }

    return dataPieces;
}

Data mergeDataPieces(const DataPieces dataPieces, const uint64_t reserved) {
    uint64_t lenSum = 0;
    for (uint64_t i = 0; i < dataPieces.len; ++i) lenSum += dataPieces.pieces[i].len - reserved;

    Data data = {(uint8_t *) calloc(lenSum, sizeof(uint8_t)), lenSum, false};
    uint64_t dataIndex = 0;
    for (uint64_t i = 0; i < dataPieces.len; ++i) {
        const Data piece = dataPieces.pieces[i];
        memcpy(data.data + dataIndex, piece.data + reserved, piece.len - reserved);
        dataIndex += piece.len - reserved;
    }

    return data;
}

int main(void) {
    ImageList imageList = createRandomImageList();
    const Data data = randData(2025);
    const uint64_t reserved = 64;

    Precomputed precomputed = precompute(imageList, data.len, reserved);
    if (precomputed.code != OK) {
        printf("Error occurred during precomputation, code: %llu\n", precomputed.code);
        free_imageList(&imageList);
        return (int) precomputed.code;
    }

    DataPieces dataPieces = splitData(precomputed, data, reserved);

    Embedded embedded = embed(precomputed, dataPieces);
    if (embedded.code != OK) {
        printf("Error occurred during embedding, code: %llu\n", precomputed.code);
        free_precomputed(precomputed);
        return (int) embedded.code;
    }

    DataPieces rDataPieces = extractImageList(embedded.imageList, reserved);
    Data rData = mergeDataPieces(rDataPieces, reserved);

    if (data.len != rData.len) {
        printf("Test failed: lengths don't match\n");
    } else if (memcmp(data.data, rData.data, data.len) != 0) {
        printf("Test failed: data don't match\n");
    } else {
        printf("Test Succeeded\n");
    }

    free_dataPieces(&dataPieces);
    free_dataPieces(&rDataPieces);
    free_data(&rData);
    free_precomputed(precomputed);

    return 0;
}