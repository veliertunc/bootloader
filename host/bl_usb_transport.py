from __future__ import annotations

from bl_hid_backend import HIDBackend
from bl_transport import BootloaderTransport


USB_HID_REPORT_SIZE = 64
USB_HID_HEADER_SIZE = 4
USB_HID_PAYLOAD_SIZE = (
    USB_HID_REPORT_SIZE -
    USB_HID_HEADER_SIZE
)

USB_HID_REPORT_SINGLE = 0
USB_HID_REPORT_START = 1
USB_HID_REPORT_CONTINUE = 2
USB_HID_REPORT_END = 3

USB_HID_DEFAULT_TIMEOUT_MS = 2000


class USBHIDTransport(BootloaderTransport):
    """
    USB HID implementation of BootloaderTransport.

    One BootloaderProtocol packet may require multiple HID reports.
    """

    def __init__(
        self,
        backend: HIDBackend,
        timeout_ms: int = USB_HID_DEFAULT_TIMEOUT_MS,
    ) -> None:
        self.backend = backend
        self.timeout_ms = timeout_ms

    def open(self) -> None:
        self.backend.open()

    def close(self) -> None:
        self.backend.close()

    def _make_report(
        self,
        report_type: int,
        sequence: int,
        payload: bytes,
    ) -> bytes:
        if len(payload) > USB_HID_PAYLOAD_SIZE:
            raise ValueError(
                "HID payload is too large"
            )

        report = bytearray(
            USB_HID_REPORT_SIZE
        )

        report[0] = report_type
        report[1] = sequence & 0xFF
        report[2] = len(payload) & 0xFF
        report[3] = (
            len(payload) >> 8
        ) & 0xFF

        report[
            USB_HID_HEADER_SIZE:
            USB_HID_HEADER_SIZE + len(payload)
        ] = payload

        return bytes(report)

    def send(
        self,
        data: bytes,
    ) -> None:
        if not data:
            raise ValueError(
                "Cannot send an empty packet"
            )

        offset = 0
        sequence = 0

        while offset < len(data):
            remaining = len(data) - offset

            chunk_length = min(
                remaining,
                USB_HID_PAYLOAD_SIZE,
            )

            chunk = data[
                offset:
                offset + chunk_length
            ]

            if (
                offset == 0
                and remaining <= USB_HID_PAYLOAD_SIZE
            ):
                report_type = (
                    USB_HID_REPORT_SINGLE
                )
            elif offset == 0:
                report_type = (
                    USB_HID_REPORT_START
                )
            elif remaining <= USB_HID_PAYLOAD_SIZE:
                report_type = (
                    USB_HID_REPORT_END
                )
            else:
                report_type = (
                    USB_HID_REPORT_CONTINUE
                )

            report = self._make_report(
                report_type,
                sequence,
                chunk,
            )

            self.backend.write(report)

            offset += chunk_length
            sequence = (
                sequence + 1
            ) & 0xFF

    def receive(self) -> bytes:
        packet = bytearray()

        expected_sequence = 0

        while True:
            report = self.backend.read(
                USB_HID_REPORT_SIZE,
                self.timeout_ms,
            )

            if not report:
                raise TimeoutError(
                    "Timed out waiting for "
                    "USB HID response"
                )

            if len(report) != USB_HID_REPORT_SIZE:
                raise RuntimeError(
                    "Invalid HID report size: "
                    f"{len(report)}"
                )

            report_type = report[0]
            sequence = report[1]

            payload_length = (
                report[2]
                | (report[3] << 8)
            )

            if payload_length > USB_HID_PAYLOAD_SIZE:
                raise RuntimeError(
                    "Invalid HID payload length"
                )

            if sequence != expected_sequence:
                raise RuntimeError(
                    "USB HID sequence error: "
                    f"expected "
                    f"{expected_sequence}, "
                    f"received {sequence}"
                )

            payload = report[
                USB_HID_HEADER_SIZE:
                USB_HID_HEADER_SIZE +
                payload_length
            ]

            if report_type == USB_HID_REPORT_SINGLE:
                if expected_sequence != 0:
                    raise RuntimeError(
                        "Invalid SINGLE report"
                    )

                packet.extend(payload)
                return bytes(packet)

            if report_type == USB_HID_REPORT_START:
                if expected_sequence != 0:
                    raise RuntimeError(
                        "Invalid START report"
                    )

                packet.extend(payload)

            elif report_type == USB_HID_REPORT_CONTINUE:
                if expected_sequence == 0:
                    raise RuntimeError(
                        "Invalid CONTINUE report"
                    )

                packet.extend(payload)

            elif report_type == USB_HID_REPORT_END:
                if expected_sequence == 0:
                    raise RuntimeError(
                        "Invalid END report"
                    )

                packet.extend(payload)
                return bytes(packet)

            else:
                raise RuntimeError(
                    "Unknown HID report type: "
                    f"0x{report_type:02X}"
                )

            expected_sequence = (
                expected_sequence + 1
            ) & 0xFF

            if len(packet) > 1024:
                raise RuntimeError(
                    "USB HID packet exceeds "
                    "maximum size"
                )