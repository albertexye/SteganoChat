import ctypes
from enum import Enum
from typing import BinaryIO, Sequence

from PIL import Image

# C data structure definitions
SQUARE_SIZE = 16


class CSquare(ctypes.Structure):
    """An internal C struct representing the data structure of a square"""

    _fields_ = (
        ('x', ctypes.c_uint64),
        ('y', ctypes.c_uint64),
        ('entropy', ctypes.c_double),
    )


class CSquareList(ctypes.Structure):
    """An internal C struct representing the data structure of **list[CSquare]**"""

    _fields_ = (
        ('squares', ctypes.POINTER(CSquare)),
        ('len', ctypes.c_uint64),
    )


class CImage(ctypes.Structure):
    """A C struct representing the data structure of **PIL.Image.Image**"""

    _fields_ = (
        ('w', ctypes.c_uint64),
        ('h', ctypes.c_uint64),
        ('c', ctypes.c_uint64),
        ('pixels', ctypes.POINTER(ctypes.c_uint8)),
        ('squareList', CSquareList),
        ('usage', ctypes.c_uint64),
        ('copied', ctypes.c_bool),
    )

    def __bytes__(self) -> bytes:
        return self.get_pixels()

    def get_pixels(self) -> bytes:
        return bytes(ctypes.cast(self.pixels, ctypes.POINTER(ctypes.c_uint8 * self.w * self.h * self.c)).contents)

    @classmethod
    def new_image(cls, image: Image.Image):
        channels = len(image.getbands())
        pixels = image.tobytes()
        buffer = (ctypes.c_uint8 * len(pixels)).from_buffer(bytearray(pixels))
        return cls(image.width, image.height, channels, buffer, CSquareList(None, 0), 0, False)


class CImageList(ctypes.Structure):
    """A C struct representing the data structure of **list[PIL.Image.Image]**"""

    _fields_ = (
        ('images', ctypes.POINTER(CImage)),
        ('len', ctypes.c_uint64),
    )

    def get_images(self) -> list[CImage]:
        return list(ctypes.cast(self.images, ctypes.POINTER(CImage * self.len)).contents)


class CData(ctypes.Structure):
    """A C struct representing the data structure of **bytes**"""

    _fields_ = (
        ('data', ctypes.POINTER(ctypes.c_uint8)),
        ('len', ctypes.c_uint64),
        ('padded', ctypes.c_bool),
    )

    def __bytes__(self):
        return self.get_data()

    def get_data(self) -> bytes:
        return bytes(ctypes.cast(self.data, ctypes.POINTER(ctypes.c_uint8 * self.len)).contents)


class CDataPieces(ctypes.Structure):
    """A C struct representing the data structure of **list[bytes]**"""

    _fields_ = (
        ('pieces', ctypes.POINTER(CData)),
        ('len', ctypes.c_uint64),
    )

    def get_pieces(self) -> list[bytes]:
        return [bytes(i) for i in list(ctypes.cast(self.pieces, ctypes.POINTER(CData * self.len)).contents)]


class CPrecomputed(ctypes.Structure):
    """A C struct representing the result of function **precompute**"""

    _fields_ = (
        ('imageList', CImageList),
        ('code', ctypes.c_uint64),
    )

    def get_lengths(self, reserved: int) -> tuple[int]:
        """Gets the lengths of data pieces (without structure size)

        :param reserved: the reserved size for structures
        :return: the lengths of pieces without structure size
        """

        result: list[int] = []
        for c_image in self.imageList.get_images():
            result.append(c_image.usage * (SQUARE_SIZE * SQUARE_SIZE * c_image.c) - reserved)
        return tuple(result)


class CExtracted(ctypes.Structure):
    """A C struct representing the result of function **extract**"""

    _fields_ = (
        ('data', CData),
        ('code', ctypes.c_uint64),
    )


# typedef CPrecomputed CEmbedded;
CEmbedded = CPrecomputed

# C function definitions
DLL = ctypes.CDLL("./libstegano.dll")

# Precomputed precompute(ImageList imageList, uint64_t dataLen, uint64_t reserved);
precompute: ctypes.CFUNCTYPE = DLL.precompute
precompute.argtypes = (CImageList, ctypes.c_uint64, ctypes.c_uint64)
precompute.restype = CPrecomputed

# Embedded embed(Precomputed precomputed, DataPieces dataPieces);
embed: ctypes.CFUNCTYPE = DLL.embed
embed.argtypes = (CPrecomputed, CDataPieces)
embed.restype = CEmbedded

# Extracted extract(Image image, uint64_t reserved);
extract: ctypes.CFUNCTYPE = DLL.extract
extract.argtypes = (CImage, ctypes.c_uint64)
extract.restype = CExtracted

# void free_precomputed(Precomputed precomputed);
free_precomputed: ctypes.CFUNCTYPE = DLL.free_precomputed
free_precomputed.argtypes = (CPrecomputed,)
free_precomputed.restype = None

# void free_extracted(Extracted extracted);
free_extracted: ctypes.CFUNCTYPE = DLL.free_extracted
free_extracted.argtypes = (CExtracted,)
free_extracted.restype = None


class CStatus(Enum):
    """The error code of the C functions"""

    OK = 0
    AllocationFailure = 1  # alloc(...) == NULL
    OversizedData = 2  # the images can't contain the data
    BadDataPiecesLen = 3  # DataPieces.len != Precomputed.imageList.len
    BadPrecomputed = 4  # a Precomputed with an error code is passed to embed
    InvalidLen = 5  # invalid length of data in the extracted image


class Steganography:
    """
    A class for embedding and extracting data from images.
    For embedding, an object has to be constructed.
    For extracting, no object needs to be constructed, instead only a class method is used.
    """

    images: list[tuple[BinaryIO, BinaryIO]]  # a list containing the images added
    modes: list[str]  # a list that stores the mode of each image
    precomputed: CPrecomputed
    is_precomputed: bool
    reserved: int  # the reserved size for data structure
    data_len: int

    def __init__(self, reserved: int):
        """
        :param reserved: the reserved size for data structure
        """

        self.images = []
        self.modes = []
        self.is_precomputed = False
        self.reserved = reserved

    @staticmethod
    def __handle_error_code(code: int):
        match code:
            case CStatus.AllocationFailure:
                raise MemoryError("memory allocation failure in CDLL")
            case CStatus.OversizedData.value:
                raise ValueError("oversized data")
            case CStatus.BadDataPiecesLen.value:
                raise ValueError("precomputed data is invalid: invalid len")
            case CStatus.BadPrecomputed:
                raise ValueError("precomputed data have an error code")
            case CStatus.InvalidLen.value:
                raise ValueError("invalid image: invalid data length")

    def add_image(self, file_src: BinaryIO, file_dst: BinaryIO) -> None:
        """Adds an image to the image list. For embedding only.

        :param file_src: the source file that is readable
        :param file_dst: the destiny file that is writable
        *file_src* may be the same object as *file_dst*.
        *file_src* and *file_dst* will not be closed until **clear** is called.
        """

        self.images.append((file_src, file_dst))
        if self.is_precomputed:
            free_precomputed(self.precomputed)
        self.is_precomputed = False

    def clear(self) -> None:
        """Clears the image list. For embedding only."""

        for src, dst in self.images:
            if not src.closed:
                src.close()
            if not dst.closed:
                dst.close()
        self.images.clear()
        self.modes.clear()
        if self.is_precomputed:
            free_precomputed(self.precomputed)
        self.is_precomputed = False

    def precompute(self, data_length: int) -> None:
        """Precomputes the images. This method has to be called before embedding.

        :param data_length: The length of the data being embedded (without structure size)
        """

        if self.is_precomputed:
            return

        self.modes.clear()
        self.data_len = data_length

        image_list = CImageList((CImage * len(self.images))(), len(self.images))

        for i, (reader, _) in enumerate(self.images):
            image = Image.open(reader)
            self.modes.append(image.mode)

            c_image = CImage.new_image(image)
            image_list.images[i] = c_image
            image.close()

        self.precomputed = precompute(image_list, data_length, self.reserved)

        self.__handle_error_code(self.precomputed.code)
        self.is_precomputed = True

    def embed(self, pieces: Sequence[bytes], format_: str = "PNG"):
        """Embeds pieces of data into the images. Call this method after calling **precompute**.

        :param pieces: the pieces of data being embedded, including the structures
        :param format_: the format for the embedded images
        """

        if not self.precomputed:
            raise RuntimeError("precomputation has not been done. Call the precompute method first.")

        # build CDataPieces struct
        data_pieces = CDataPieces((CData * len(pieces))(), len(pieces))
        for i in range(len(pieces)):
            piece = pieces[i]
            buffer = (ctypes.c_uint8 * len(piece)).from_buffer(bytearray(piece))
            data_pieces.pieces[i] = CData(buffer, len(piece))

        # call embedded
        embedded: CEmbedded = embed(self.precomputed, data_pieces)

        # handle errors
        self.__handle_error_code(embedded.code)

        # save results
        for i, c_image in enumerate(embedded.imageList.get_images()):
            c_image: CImage
            image = Image.frombuffer(self.modes[i], (c_image.w, c_image.h), c_image.get_pixels())
            writer = self.images[i][1]
            writer.truncate(0)
            image.save(writer, format_)
            image.close()

    @classmethod
    def extract(cls, src: BinaryIO, reserved: int) -> bytes:
        """Extracts the data from **src**

        **src** will not be closed, you have to close it somewhere
        :param src: the source image
        :param reserved: the reserved size for structure
        :return: the extracted data
        """

        image = Image.open(src)
        c_image = CImage.new_image(image)
        extracted: CExtracted = extract(c_image, ctypes.c_uint64(reserved))
        image.close()

        cls.__handle_error_code(extracted.code)

        result = bytes(extracted.data)
        free_extracted(extracted)
        return result
