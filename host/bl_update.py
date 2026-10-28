from __future__ import annotations

import argparse
import struct
import sys
import time
from pathlib import Path

import serial
import serial.tools.list_ports

from bl_protocol import (
    BootloaderError,
    BootloaderProtocol,
    IMAGE_HEADER_ADDRESS,
    IMAGE_HEADER_SIZE,
    APPLICATION_ADDRESS,
)
from bl_i2c_transport import I2CTransport
from bl_transport import BootloaderTransport


IMAGE_MAGIC = 0x544D3443
IMAGE_HEADER_VERSION = 0x00010000

APPLICATION_IMAGE_OFFSET = 0x400

WRITE_CHUNK_SIZE = 224


def print_ports() -> None:
    ports = list(serial.tools.list_ports.comports())

    if not ports:
        print("No serial ports found.")
        return

    print("Available serial ports:")

    for port in ports:
        description = port.description or ""

        print(
            f"  {port.device:<8} "
            f"{description}"
        )


def read_image(
    path: Path,
) -> tuple[bytes, bytes]:

    data = path.read_bytes()

    if len(data) < APPLICATION_IMAGE_OFFSET:
        raise ValueError(
            "Application image is too small"
        )

    header = data[:IMAGE_HEADER_SIZE]

    reserved = data[
        IMAGE_HEADER_SIZE:APPLICATION_IMAGE_OFFSET
    ]

    application = data[
        APPLICATION_IMAGE_OFFSET:
    ]

    if len(header) != 32:
        raise ValueError(
            "Invalid header size"
        )

    if any(byte != 0xFF for byte in reserved):
        raise ValueError(
            "Reserved image area is not erased (0xFF)"
        )

    (
        magic,
        header_version,
        image_version,
        image_size,
        image_crc,
        reserved0,
        reserved1,
        reserved2,
    ) = struct.unpack(
        "<8I",
        header,
    )

    if magic != IMAGE_MAGIC:
        raise ValueError(
            f"Invalid image magic: "
            f"0x{magic:08X}"
        )

    if header_version != IMAGE_HEADER_VERSION:
        raise ValueError(
            f"Unsupported image header version: "
            f"0x{header_version:08X}"
        )

    if image_size != len(application):
        raise ValueError(
            f"Image size mismatch: "
            f"header={image_size}, "
            f"file={len(application)}"
        )

    if image_size == 0:
        raise ValueError(
            "Application image is empty"
        )

    if image_size % 4 != 0:
        raise ValueError(
            "Application image size must be 4-byte aligned"
        )

    print(
        f"Image version : "
        f"{(image_version >> 24) & 0xFF}."
        f"{(image_version >> 16) & 0xFF}."
        f"{image_version & 0xFFFF}"
    )

    print(
        f"Image size    : "
        f"{image_size} bytes"
    )

    print(
        f"Image CRC32   : "
        f"0x{image_crc:08X}"
    )

    print(
        f"Header        : "
        f"0x{IMAGE_HEADER_ADDRESS:08X}"
    )

    print(
        f"Application   : "
        f"0x{APPLICATION_ADDRESS:08X}"
    )

    return header, application


def print_progress(
    current: int,
    total: int,
) -> None:

    if total == 0:
        percent = 100
    else:
        percent = (
            current * 100
        ) // total

    width = 40

    filled = (
        width * current
    ) // max(total, 1)

    bar = (
        "#" * filled
        + "-" * (width - filled)
    )

    print(
        f"\r[{bar}] "
        f"{percent:3d}% "
        f"({current}/{total})",
        end="",
        flush=True,
    )


def update(
    port: str | None,
    image_path: Path,
    ethernet_host: str | None = None,
    ethernet_port: int = 5000,
    i2c_bus: int | None = None,
    transport: BootloaderTransport | None = None,
) -> None:

    header, application = read_image(
        image_path
    )
    _, _, _, image_size, image_crc, _, _, _ = struct.unpack("<8I", header)

    print()
    selected_transport = transport
    if transport is not None:
        print("Opening SPI transport...")
    elif i2c_bus is not None:
        print(f"Opening I2C bus {i2c_bus} at slave address 0x42...")
        selected_transport = I2CTransport(bus=i2c_bus)
    elif ethernet_host is not None:
        print(f"Opening W5500 UDP at {ethernet_host}:{ethernet_port}...")
    else:
        print(f"Opening {port} @ 115200...")

    with BootloaderProtocol(
        port=selected_transport if selected_transport is not None else port,
        baudrate=115200,
        timeout=1.0,
        ethernet_host=ethernet_host,
        ethernet_port=ethernet_port,
    ) as bl:

        print("Connecting...")

        bl.connect()

        version = bl.get_version()

        print(
            "Bootloader version: "
            f"{version[0]}."
            f"{version[1]}."
            f"{version[2]}"
        )

        info = bl.get_info()

        print(
            f"Flash: "
            f"0x{info['flash_start']:08X} - "
            f"0x{info['flash_end']:08X}"
        )

        print(
            f"Application: "
            f"0x{info['app_start']:08X} - "
            f"0x{info['app_end']:08X}"
        )

        print(
            f"Maximum application size: "
            f"{info['app_max_size']} bytes"
        )

        if len(application) > info["app_max_size"]:
            raise ValueError(
                "Application does not fit target"
            )

        print()
        print("Erasing update area...")

        bl.erase(
            0x00008000,
            0x00038000,
        )

        print("Erase complete.")

        print()
        print("Writing image header...")

        bl.write(
            IMAGE_HEADER_ADDRESS,
            header,
        )

        print("Header written.")

        print()
        print(
            f"Writing application "
            f"({len(application)} bytes)..."
        )

        address = APPLICATION_ADDRESS
        offset = 0

        while offset < len(application):

            chunk = application[
                offset:
                offset + WRITE_CHUNK_SIZE
            ]

            bl.write(
                address + offset,
                chunk,
            )

            offset += len(chunk)

            print_progress(
                offset,
                len(application),
            )

        print()

        print()
        print("Verifying application...")

        bl.verify(image_size, image_crc)

        print("Verification successful.")

        print()
        print("Starting application...")

        try:
            bl.run_application()

        except (
            serial.SerialException,
            BootloaderError,
            OSError,
        ):
            # RUN_APP causes the MCU to leave the bootloader.
            # Losing the response is acceptable if the target has
            # already started the application.

            pass

        time.sleep(0.2)

    print()
    print("Update complete.")


def main() -> int:

    parser = argparse.ArgumentParser(
        description="TM4C123 bootloader updater"
    )

    parser.add_argument(
        "--port",
        help="Serial port, e.g. COM5",
    )

    parser.add_argument(
        "--image",
        help="application_image.bin",
    )

    parser.add_argument(
        "--ethernet",
        help="W5500 target IPv4 address (default target is 192.168.1.50)",
    )

    parser.add_argument(
        "--udp-port",
        type=int,
        default=5000,
        help="W5500 bootloader UDP port (default: 5000)",
    )

    parser.add_argument(
        "--i2c-bus",
        type=int,
        help="Linux I2C bus number (slave address 0x42)",
    )

    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="List available serial ports",
    )

    args = parser.parse_args()

    if args.list_ports:
        print_ports()
        return 0

    if sum((bool(args.port), bool(args.ethernet), args.i2c_bus is not None)) != 1:
        parser.error("specify exactly one of --port, --ethernet, or --i2c-bus")

    if not args.image:
        parser.error("--image is required")

    image_path = Path(args.image)

    if not image_path.exists():
        print(
            f"ERROR: Image not found: {image_path}",
            file=sys.stderr,
        )

        return 1

    try:
        update(
            port=args.port,
            image_path=image_path,
            ethernet_host=args.ethernet,
            ethernet_port=args.udp_port,
            i2c_bus=args.i2c_bus,
        )

    except (
        BootloaderError,
        ValueError,
        OSError,
    ) as exc:

        print()
        print(
            f"ERROR: {exc}",
            file=sys.stderr,
        )

        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
