import os
import time
from typing import Sequence


def split(data: bytes, chunks: Sequence[int]) -> tuple[bytes, ...]:
    """Splits data into chunks

    :param data: the data that need to be split
    :param chunks: the lengths of chunks that need to be split into
    :return: the split chunks
    """

    # the id of the message
    # it should be unique each time, but since the id used is not recorded,
    # it should be fine to randomly generate an 8-byte id
    id_ = os.urandom(8)
    # the timestamp at the time the message is sent
    timestamp = int(time.time())

    # the index of data
    data_index = 0
    # the total chunk number
    total = len(chunks)
    result = []

    for i in range(len(chunks)):
        chunk = chunks[i]
        # the data structure:
        #   id - 8 bytes, a unique uint64 for the message
        #   total - 8 bytes, the total chunk number, for the receiver to know how many pieces are missing
        #   index - 8 bytes, the index in the data, for the receiver to putting the pieces together
        #   time - 8 bytes, the time the message is generated
        #   data - the chunk of data
        result.append(b"".join((
            id_,
            total.to_bytes(8, "little", signed=False),
            i.to_bytes(8, "little", signed=False),
            timestamp.to_bytes(8, "little", signed=False),
            data[data_index:data_index + chunk]
        )))
        data_index += chunk

    return tuple(result)


def check(chunks: Sequence[bytes]) -> dict[tuple[bytes, int, int], list[tuple[int, bytes]]]:
    """Gets a summary of the metadata of the chunks

    :param chunks: a list of chunks
    :return: a dict that represents the metadata. format: {(id, total, time): [(index, chunk), ...], ...}
    """

    # each message has a signature, which consists the message id, the total chunk number, and the sending time.
    # the signature of each message should be unique
    ids = {}
    timestamp_now = int(time.time())
    for i in range(len(chunks)):
        chunk = chunks[i]
        if len(chunk) < 32:
            raise ValueError(f"invalid chunk size: {len(chunk)} < 32")
        id_: bytes = chunk[:8]
        total = int.from_bytes(chunk[8:16], "little", signed=False)
        index = int.from_bytes(chunk[16:24], "little", signed=False)
        timestamp = int.from_bytes(chunk[24:32], "little", signed=False)

        # when the message is from the future
        if timestamp > timestamp_now:
            raise ValueError(f"invalid timestamp in the message ({timestamp} > {timestamp_now})")
        # when the index is invalid
        if index >= total:
            raise ValueError(f"the index of a chunk can't be greater the the total length ({index} >= {total})")

        if (id_, total, timestamp) in ids:
            ids[(id_, total, timestamp)].append((index, chunk))
        else:
            ids[(id_, total, timestamp)] = [(index, chunk)]

    # last step, sort the chunks
    for signature in ids:
        ids[signature].sort()

    return ids


def merge(chunks: Sequence[bytes]) -> bytes:
    """Merges the chunks together. If there are conflicts, an exception is raised

    :param chunks: the chunks
    :return: the merged data
    """

    if len(chunks) == 0:
        raise ValueError("chunks can't be empty")

    checked = check(chunks)
    if len(checked) > 1:
        raise ValueError("more than one message in chunks")

    signature = next(iter(checked))
    indexes = checked[signature]

    if len(indexes) - 1 > indexes[-1][0]:
        raise ValueError("incomplete message")
    if len(indexes) != len(set(indexes)):
        raise ValueError("repeated index in a message")

    result = []
    for _, piece in indexes:
        result.append(piece[32:])

    return b"".join(result)
