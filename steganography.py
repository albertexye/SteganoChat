import ctypes
import math
from typing import BinaryIO

from PIL import Image

SQUARE_SIZE = 8

DLL = ctypes.CDLL("./stegano.dll")

embed: ctypes.CFUNCTYPE = DLL.embed
embed.argtypes = (ctypes.c_char_p, ctypes.c_uint64, ctypes.c_char_p, ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64)
embed.restype = ctypes.c_void_p

extract = DLL.extract
extract.argtypes = (ctypes.c_char_p, ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64)
extract.restype = ctypes.c_void_p

get_data = DLL.get_data
get_data.argtypes = (ctypes.c_void_p,)
get_data.restype = ctypes.POINTER(ctypes.c_char)

get_len = DLL.get_len
get_len.argtypes = (ctypes.c_void_p,)
get_len.restype = ctypes.c_uint64

free = DLL.free
free.argtypes = (ctypes.c_void_p,)


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
            if not src.closed:
                src.close()
            if not dst.closed:
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
            length = int((int(image.width / SQUARE_SIZE) * int(
                image.height / SQUARE_SIZE) - 1) * SQUARE_SIZE * SQUARE_SIZE * channels / 8)
            entropy = image.entropy()
            entropy_sum += entropy
            precomputed.append((entropy, length, image))

        passed = False
        while not passed:
            passed = True
            for entropy, length, image in precomputed:
                if length < math.ceil(data_length * (entropy / entropy_sum)) + self.structure_length:
                    passed = False
                    entropy_sum -= entropy
                    image.close()
                    precomputed.remove((entropy, length, image))
                    break

        for entropy, length, image in precomputed:
            self.precomputed.append((math.ceil(data_length * (entropy / entropy_sum)), image))

    def embed(self, pieces: list[bytes], format_: str = "PNG"):
        if not self.precomputed:
            raise RuntimeError("Precomputation has not been done. Call the precompute method first.")
        for i in range(len(pieces)):
            if len(pieces[i]) != self.precomputed[i][0] + self.structure_length:
                raise ValueError(f"The length of pieces[{i}] is not correct. ")

        for i in range(len(pieces)):
            image = self.precomputed[i][1]
            channels = len(image.getbands())
            raw = embed(
                pieces[i],
                ctypes.c_uint64(len(pieces[i])),
                image.tobytes(),
                ctypes.c_uint64(image.width),
                ctypes.c_uint64(image.height),
                ctypes.c_uint64(channels)
            )
            image.close()
            embedded = bytes(
                ctypes.cast(raw, ctypes.POINTER(ctypes.c_char * (image.width * image.height * channels))).contents)
            free(raw)

            new_image = Image.frombuffer(image.mode, image.size, embedded)
            self.images[i][1].truncate(0)
            new_image.save(self.images[i][1], format_)
            self.images[i][1].close()
            new_image.close()

    @staticmethod
    def extract(src: BinaryIO) -> bytes:
        image = Image.open(src)
        data: ctypes.c_void_p = extract(image.tobytes(), image.width, image.height, len(image.getbands()))
        image.close()
        src.close()
        result_len = get_len(data)
        result_ptr = get_data(data)
        result = bytes(ctypes.cast(result_ptr, ctypes.POINTER(ctypes.c_char * result_len)).contents)
        free(data)
        return result
