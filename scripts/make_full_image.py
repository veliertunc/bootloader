#!/usr/bin/env python3

import argparse
from pathlib import Path


BOOTLOADER_SIZE = 0x8000
APPLICATION_FLASH_ADDRESS = 0x8000


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create complete TM4C123 flash binary"
    )

    parser.add_argument(
        "--bootloader",
        required=True,
    )

    parser.add_argument(
        "--application",
        required=True,
    )

    parser.add_argument(
        "--output",
        required=True,
    )

    args = parser.parse_args()

    bootloader_path = Path(args.bootloader)
    application_path = Path(args.application)
    output_path = Path(args.output)

    bootloader = bootloader_path.read_bytes()
    application = application_path.read_bytes()

    if len(bootloader) > BOOTLOADER_SIZE:
        raise RuntimeError(
            f"Bootloader is too large: "
            f"{len(bootloader)} bytes > "
            f"{BOOTLOADER_SIZE} bytes"
        )

    if len(application) == 0:
        raise RuntimeError("Application image is empty")

    # Fill unused bootloader Flash with erased Flash.
    bootloader_padded = (
        bootloader
        + b"\xFF" * (BOOTLOADER_SIZE - len(bootloader))
    )

    full_image = bootloader_padded + application

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    output_path.write_bytes(full_image)

    print("Full Flash image generated")
    print(f"  Bootloader : {len(bootloader)} bytes")
    print(f"  App image  : {len(application)} bytes")
    print(f"  Total      : {len(full_image)} bytes")
    print("  Boot addr  : 0x00000000")
    print("  App addr   : 0x00008000")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())