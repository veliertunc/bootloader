import struct
import zlib


SPI_MAGIC = 0xAA55AA55

HEADER_SIZE = 8
CRC_SIZE = 4

def encode_frame(
        payload: bytes) -> bytes:

    crc = zlib.crc32(payload)

    header = struct.pack(
        "<II",
        SPI_MAGIC,
        len(payload))

    trailer = struct.pack(
        "<I",
        crc)

    return (
        header +
        payload +
        trailer
    )

def decode_frame(
        frame: bytes) -> bytes:

    if len(frame) < 12:
        raise ValueError(
            "Frame too short")

    magic, length = \
        struct.unpack(
            "<II",
            frame[:8])

    if magic != SPI_MAGIC:
        raise ValueError(
            "Invalid magic")

    payload = frame[
        8:
        8 + length
    ]

    received_crc = \
        struct.unpack(
            "<I",
            frame[
                8 + length:
                12 + length
            ]
        )[0]

    calculated_crc = \
        zlib.crc32(payload)

    if received_crc != calculated_crc:
        raise ValueError(
            "CRC mismatch")

    return payload
