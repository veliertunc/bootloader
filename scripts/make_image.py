#!/usr/bin/env python3

import argparse
import struct
import zlib
from pathlib import Path


IMAGE_MAGIC = 0x544D3443
IMAGE_HEADER_VERSION = 0x00010000

APP_HEADER_SIZE = 32
APP_VECTOR_OFFSET = 0x400


def parse_integer(value: str) -> int:
    return int(value, 0)


def pad_to_word(data: bytes) -> bytes:
    remainder = len(data) % 4

    if remainder == 0:
        return data

    return data + (b"\xFF" * (4 - remainder))


def calculate_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def make_header(
    image_version: int,
    image_size: int,
    image_crc: int,
) -> bytes:

    return struct.pack(
        "<8I",
        IMAGE_MAGIC,
        IMAGE_HEADER_VERSION,
        image_version,
        image_size,
        image_crc,
        0,
        0,
        0,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate TM4C123 bootloader application image"
    )

    parser.add_argument(
        "--input",
        required=True,
        help="Application binary",
    )

    parser.add_argument(
        "--header",
        required=True,
        help="Output image header binary",
    )

    parser.add_argument(
        "--output",
        required=True,
        help="Output combined image binary",
    )

    parser.add_argument(
        "--version",
        default="1.0.0",
        help="Application version, e.g. 1.2.3",
    )

    args = parser.parse_args()

    input_path = Path(args.input)
    header_path = Path(args.header)
    output_path = Path(args.output)

    if not input_path.exists():
        raise FileNotFoundError(
            f"Input file does not exist: {input_path}"
        )

    version_parts = args.version.split(".")

    if len(version_parts) != 3:
        raise ValueError(
            "Version must have format MAJOR.MINOR.PATCH"
        )

    major = int(version_parts[0])
    minor = int(version_parts[1])
    patch = int(version_parts[2])

    if not 0 <= major <= 255:
        raise ValueError("Major version out of range")

    if not 0 <= minor <= 255:
        raise ValueError("Minor version out of range")

    if not 0 <= patch <= 65535:
        raise ValueError("Patch version out of range")

    image_version = (
        (major << 24)
        | (minor << 16)
        | patch
    )

    application = input_path.read_bytes()

    application = pad_to_word(application)

    image_size = len(application)
    image_crc = calculate_crc32(application)

    header = make_header(
        image_version=image_version,
        image_size=image_size,
        image_crc=image_crc,
    )

    if len(header) != APP_HEADER_SIZE:
        raise RuntimeError("Invalid image header size")

    reserved_size = APP_VECTOR_OFFSET - APP_HEADER_SIZE

    combined = (
        header
        + (b"\xFF" * reserved_size)
        + application
    )

    header_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    header_path.write_bytes(header)
    output_path.write_bytes(combined)

    print("Application image generated")
    print(f"  Input       : {input_path}")
    print(f"  Header      : {header_path}")
    print(f"  Output      : {output_path}")
    print(f"  Version     : {major}.{minor}.{patch}")
    print(f"  Image size  : {image_size} bytes")
    print(f"  Image CRC32 : 0x{image_crc:08X}")
    print(f"  Total image : {len(combined)} bytes")
    print("  Flash base  : 0x00008000")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())