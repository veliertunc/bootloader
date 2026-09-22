from __future__ import annotations

import argparse
from pathlib import Path

from bl_hid_backend import HIDBackend
from bl_usb_config import (
    BOOTLOADER_PRODUCT_ID,
    BOOTLOADER_VENDOR_ID,
    USB_HID_TIMEOUT_MS,
)
from bl_usb_transport import USBHIDTransport

from bl_protocol import BootloaderProtocol


def list_devices() -> None:
    devices = HIDBackend.enumerate(
        BOOTLOADER_VENDOR_ID,
        BOOTLOADER_PRODUCT_ID,
    )

    if not devices:
        print(
            "No bootloader HID devices found."
        )
        return

    for index, device in enumerate(
        devices
    ):
        print(
            f"[{index}] "
            f"VID=0x{device.vendor_id:04X} "
            f"PID=0x{device.product_id:04X} "
            f"product={device.product!r} "
            f"serial={device.serial_number!r}"
        )


def update(image_path: Path) -> None:
    backend = HIDBackend(
        vendor_id=BOOTLOADER_VENDOR_ID,
        product_id=BOOTLOADER_PRODUCT_ID,
    )

    transport = USBHIDTransport(
        backend,
        timeout_ms=USB_HID_TIMEOUT_MS,
    )

    protocol = BootloaderProtocol(
        transport
    )

    transport.open()

    try:
        print("Connecting...")

        protocol.connect()

        version = protocol.get_version()

        print(
            f"Bootloader version: "
            f"{version}"
        )

        info = protocol.get_info()

        print(
            f"Application capacity: "
            f"{info}"
        )

        print(
            f"Updating from: "
            f"{image_path}"
        )

        protocol.erase()

        protocol.write_file(
            str(image_path)
        )

        protocol.verify()

        print(
            "Verification successful."
        )

        try:
            protocol.run_application()
        except (
            ConnectionError,
            TimeoutError,
        ):
            # Expected when the bootloader
            # immediately jumps to the application.
            pass

        print(
            "Application started."
        )

    finally:
        transport.close()


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "TM4C123 USB HID "
            "bootloader updater"
        )
    )

    parser.add_argument(
        "--list",
        action="store_true",
        help="List bootloader HID devices",
    )

    parser.add_argument(
        "--image",
        type=Path,
        help="Application image",
    )

    args = parser.parse_args()

    if args.list:
        list_devices()
        return

    if args.image is None:
        parser.error(
            "--image is required"
        )

    update(args.image)


if __name__ == "__main__":
    main()