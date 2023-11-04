import ctypes
import math
from typing import BinaryIO

from PIL import Image

DLL = ctypes.CDLL("./stegano.so")


class Steganography:
    images: list[tuple[BinaryIO, BinaryIO]]
    precomputed: list[tuple[int, Image.Image]]
    is_precomputed: bool
    structure_length: int

    def __init__(self, structure_length: int):
        self.images = []
        self.precomputed = []
        self.is_precomputed = False
        self.structure_length = structure_length

    def add_image(self, file_src: BinaryIO, file_dst: BinaryIO) -> None:
        self.images.append((file_src, file_dst))
        self.is_precomputed = False

    def clear(self) -> None:
        for src, dst in self.images:
            src.close()
            dst.close()
        self.images.clear()
        if self.is_precomputed:
            for _, image in self.precomputed:
                image.close()
            self.is_precomputed = False

    def precompute(self, data_length: int) -> None:
        if self.is_precomputed:
            return
        self.is_precomputed = True

        precomputed = []
        entropy_sum = 0.
        for reader, _ in self.images:
            image = Image.open(reader)
            channels = len(image.getbands())
            length = int(image.width * image.height * channels / 8)
            entropy = image.entropy()
            entropy_sum += entropy
            precomputed.append((entropy, length, image))

        passed = False
        while not passed:
            passed = True
            for entropy, length, image in precomputed:
                if length < math.ceil(data_length * (entropy / entropy_sum)) + self.structure_length + 8:
                    passed = False
                    entropy_sum -= entropy
                    image.close()
                    precomputed.remove((entropy, length, image))
                    break

        for entropy, length, image in precomputed:
            self.precomputed.append((math.ceil(data_length * (entropy / entropy_sum)), image))

    def embed(self, pieces: list[bytes]):
        if not self.precomputed:
            raise RuntimeError("Precomputation has not been done. Call the precompute method first.")
        for i in range(len(pieces)):
            if len(pieces[i]) != self.precomputed[i][0] + self.structure_length:
                raise ValueError(f"The length of pieces[{i}] is not correct. ")

        for i in range(len(pieces)):
            image = self.precomputed[i][1]
            DLL.embed(
                ctypes.c_char_p(pieces[i]),
                ctypes.c_char_p(image.tobytes()),
                ctypes.c_uint64(image.width),
                ctypes.c_uint64(image.height),
                ctypes.c_uint64(len(image.getbands()))
            )
