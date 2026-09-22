from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable

import hid


@dataclass(frozen=True)
class HIDDeviceInfo:
    """
    Information about a USB HID device.
    """

    path: str
    vendor_id: int
    product_id: int
    manufacturer: str
    product: str
    serial_number: str


class HIDBackend:
    """
    Thin wrapper around hidapi.

    No bootloader protocol logic belongs here.
    """

    def __init__(
        self,
        vendor_id: int,
        product_id: int,
        interface_number: int | None = None,
        path: bytes | str | None = None,
    ) -> None:
        self.vendor_id = vendor_id
        self.product_id = product_id
        self.interface_number = interface_number
        self.path = path

        self._device: hid.device | None = None

    @staticmethod
    def enumerate(
        vendor_id: int | None = None,
        product_id: int | None = None,
    ) -> list[HIDDeviceInfo]:
        """
        Enumerate HID devices.
        """

        devices: Iterable[dict] = hid.enumerate(
            vendor_id or 0,
            product_id or 0,
        )

        result: list[HIDDeviceInfo] = []

        for device in devices:
            result.append(
                HIDDeviceInfo(
                    path=device["path"],
                    vendor_id=device["vendor_id"],
                    product_id=device["product_id"],
                    manufacturer=device.get(
                        "manufacturer_string",
                        "",
                    )
                    or "",
                    product=device.get(
                        "product_string",
                        "",
                    )
                    or "",
                    serial_number=device.get(
                        "serial_number",
                        "",
                    )
                    or "",
                )
            )

        return result

    def open(self) -> None:
        if self._device is not None:
            return

        self._device = hid.device()

        if self.path is not None:
            self._device.open_path(self.path)
        else:
            self._device.open(
                self.vendor_id,
                self.product_id,
            )

        self._device.set_nonblocking(False)

    def close(self) -> None:
        if self._device is None:
            return

        self._device.close()
        self._device = None

    def write(self, report: bytes) -> int:
        """
        Write one HID report.

        hidapi expects the report buffer to contain the
        report ID byte when the device uses one.
        """

        if self._device is None:
            raise RuntimeError(
                "HID device is not open"
            )

        return self._device.write(
            report
        )

    def read(
        self,
        size: int,
        timeout_ms: int,
    ) -> bytes:
        """
        Read one HID report.
        """

        if self._device is None:
            raise RuntimeError(
                "HID device is not open"
            )

        data = self._device.read(
            size,
            timeout_ms,
        )

        return bytes(data)

    def get_manufacturer_string(self) -> str:
        if self._device is None:
            raise RuntimeError(
                "HID device is not open"
            )

        return (
            self._device
            .get_manufacturer_string()
            or ""
        )

    def get_product_string(self) -> str:
        if self._device is None:
            raise RuntimeError(
                "HID device is not open"
            )

        return (
            self._device
            .get_product_string()
            or ""
        )

    def get_serial_number_string(self) -> str:
        if self._device is None:
            raise RuntimeError(
                "HID device is not open"
            )

        return (
            self._device
            .get_serial_number_string()
            or ""
        )

    def __enter__(self) -> "HIDBackend":
        self.open()
        return self

    def __exit__(
        self,
        exc_type,
        exc_value,
        traceback,
    ) -> None:
        self.close()