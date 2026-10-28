from __future__ import annotations

import argparse
from pathlib import Path

from bl_spi_backend import SPIBackend
from bl_spi_transport import SPITransport
from bl_spidev_backend import LinuxSPIBackend
from bl_protocol import BootloaderError
from bl_update import update


def main() -> int:
    parser = argparse.ArgumentParser(description="Update firmware over SPI")
    parser.add_argument("--image", required=True, help="application_image.bin")
    parser.add_argument("--bus", type=int, default=0)
    parser.add_argument("--device", type=int, default=0)
    parser.add_argument("--speed", type=int, default=1_000_000)
    args = parser.parse_args()

    image_path = Path(args.image)
    if not image_path.is_file():
        parser.error(f"image file not found: {image_path}")

    backend: SPIBackend = LinuxSPIBackend(
        bus=args.bus,
        device=args.device,
        speed_hz=args.speed,
    )
    try:
        update(
            port=None,
            image_path=image_path,
            transport=SPITransport(backend),
        )
        return 0
    except (BootloaderError, OSError, ValueError) as exc:
        parser.exit(1, f"Update failed: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
