from __future__ import annotations

import struct
import time
from dataclasses import dataclass

import serial


# ---------------------------------------------------------------------------
# Protocol constants
# ---------------------------------------------------------------------------

SOF = 0xAA55

MAX_PAYLOAD_SIZE = 240
HEADER_SIZE = 6
CRC_SIZE = 4
MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE + CRC_SIZE


# Commands

CMD_CONNECT = 0x01
CMD_DISCONNECT = 0x02
CMD_GET_VERSION = 0x03
CMD_GET_INFO = 0x04

CMD_ERASE = 0x10
CMD_WRITE = 0x11
CMD_VERIFY = 0x12
CMD_ABORT = 0x13

CMD_RUN_APP = 0x20
CMD_RESET = 0x21


# Responses

CMD_ACK = 0x80
CMD_NACK = 0x81
CMD_BUSY = 0x82
CMD_CRC_ERROR = 0x83
CMD_SEQUENCE_ERROR = 0x84
CMD_FLASH_ERROR = 0x85
CMD_VERIFY_ERROR = 0x86
CMD_INVALID_CMD = 0x87


# Flash layout

UPDATE_START_ADDRESS = 0x00008000
UPDATE_END_ADDRESS = 0x0003FFFF

IMAGE_HEADER_ADDRESS = 0x00008000
IMAGE_HEADER_SIZE = 32

APPLICATION_ADDRESS = 0x00008400


# ---------------------------------------------------------------------------
# Exceptions
# ---------------------------------------------------------------------------

class BootloaderError(Exception):
    """Base bootloader exception."""


class BootloaderTimeout(BootloaderError):
    """No response received from target."""


class BootloaderProtocolError(BootloaderError):
    """Malformed protocol response."""


class BootloaderCommandError(BootloaderError):
    """Target rejected a command."""

    def __init__(
        self,
        command: int,
        response: int,
        payload: bytes,
    ):
        self.command = command
        self.response = response
        self.payload = payload

        super().__init__(
            f"Command 0x{command:02X} failed with "
            f"response 0x{response:02X}"
        )


# ---------------------------------------------------------------------------
# Packet
# ---------------------------------------------------------------------------

@dataclass
class Packet:
    command: int
    sequence: int
    payload: bytes


# ---------------------------------------------------------------------------
# CRC32
# ---------------------------------------------------------------------------

def crc32(data: bytes) -> int:
    """
    Reflected CRC-32.

    Polynomial:
        0xEDB88320

    This must match BL_CRC32_Calculate() in the firmware.
    """

    crc = 0xFFFFFFFF

    for byte in data:
        crc ^= byte

        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1

    return (~crc) & 0xFFFFFFFF


# ---------------------------------------------------------------------------
# Packet serialization
# ---------------------------------------------------------------------------

def encode_packet(
    command: int,
    sequence: int,
    payload: bytes,
) -> bytes:

    if len(payload) > MAX_PAYLOAD_SIZE:
        raise ValueError(
            f"Payload too large: {len(payload)}"
        )

    header = struct.pack(
        "<HBBH",
        SOF,
        command,
        sequence,
        len(payload),
    )

    packet_without_crc = header + payload

    packet_crc = crc32(packet_without_crc)

    return (
        packet_without_crc
        + struct.pack("<I", packet_crc)
    )


def decode_packet(data: bytes) -> Packet:
    if len(data) < HEADER_SIZE + CRC_SIZE:
        raise BootloaderProtocolError(
            "Packet too short"
        )

    sof, command, sequence, payload_length = struct.unpack(
        "<HBBH",
        data[:HEADER_SIZE],
    )

    if sof != SOF:
        raise BootloaderProtocolError(
            f"Invalid SOF: 0x{sof:04X}"
        )

    expected_length = (
        HEADER_SIZE
        + payload_length
        + CRC_SIZE
    )

    if len(data) != expected_length:
        raise BootloaderProtocolError(
            f"Invalid packet length: "
            f"expected {expected_length}, "
            f"received {len(data)}"
        )

    payload_end = HEADER_SIZE + payload_length

    payload = data[
        HEADER_SIZE:payload_end
    ]

    received_crc = struct.unpack(
        "<I",
        data[payload_end:payload_end + CRC_SIZE],
    )[0]

    calculated_crc = crc32(
        data[:payload_end]
    )

    if received_crc != calculated_crc:
        raise BootloaderProtocolError(
            f"CRC mismatch: "
            f"received 0x{received_crc:08X}, "
            f"calculated 0x{calculated_crc:08X}"
        )

    return Packet(
        command=command,
        sequence=sequence,
        payload=payload,
    )


# ---------------------------------------------------------------------------
# Bootloader transport
# ---------------------------------------------------------------------------

class BootloaderProtocol:
    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        timeout: float = 1.0,
    ):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout

        self.serial: serial.Serial | None = None

        self.sequence = 0

    # -----------------------------------------------------------------------
    # Connection
    # -----------------------------------------------------------------------

    def open(self) -> None:
        self.serial = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=self.timeout,
            write_timeout=self.timeout,
        )

        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()

    def close(self) -> None:
        if self.serial is not None:
            self.serial.close()
            self.serial = None

    def __enter__(self) -> "BootloaderProtocol":
        self.open()
        return self

    def __exit__(
        self,
        exc_type,
        exc_value,
        traceback,
    ) -> None:
        self.close()

    # -----------------------------------------------------------------------
    # Low-level receive
    # -----------------------------------------------------------------------

    def _read_exact(self, length: int) -> bytes:
        if self.serial is None:
            raise BootloaderProtocolError(
                "Serial port is not open"
            )

        result = bytearray()

        while len(result) < length:
            chunk = self.serial.read(
                length - len(result)
            )

            if not chunk:
                raise BootloaderTimeout(
                    "Timed out waiting for target"
                )

            result.extend(chunk)

        return bytes(result)

    def _receive_packet(self) -> Packet:
        """
        Synchronize on 0x55 0xAA and receive a complete packet.
        """

        if self.serial is None:
            raise BootloaderProtocolError(
                "Serial port is not open"
            )

        # Search for SOF.
        while True:
            first = self._read_exact(1)[0]

            if first != 0x55:
                continue

            second = self._read_exact(1)[0]

            if second == 0xAA:
                break

        remaining_header = self._read_exact(4)

        header = bytes([
            0x55,
            0xAA,
        ]) + remaining_header

        _, command, sequence, payload_length = struct.unpack(
            "<HBBH",
            header,
        )

        if payload_length > MAX_PAYLOAD_SIZE:
            raise BootloaderProtocolError(
                f"Target returned oversized payload: "
                f"{payload_length}"
            )

        remaining = self._read_exact(
            payload_length + CRC_SIZE
        )

        return decode_packet(
            header + remaining
        )

    # -----------------------------------------------------------------------
    # Command transaction
    # -----------------------------------------------------------------------

    def transact(
        self,
        command: int,
        payload: bytes = b"",
        retries: int = 3,
    ) -> Packet:

        if self.serial is None:
            raise BootloaderProtocolError(
                "Serial port is not open"
            )

        sequence = self.sequence

        packet = encode_packet(
            command,
            sequence,
            payload,
        )

        last_exception: Exception | None = None

        for attempt in range(retries):
            try:
                self.serial.write(packet)
                self.serial.flush()

                response = self._receive_packet()

                if response.sequence != sequence:
                    raise BootloaderProtocolError(
                        "Sequence mismatch: "
                        f"expected {sequence}, "
                        f"received {response.sequence}"
                    )

                if response.command == CMD_ACK:
                    self.sequence = (
                        self.sequence + 1
                    ) & 0xFF

                    return response

                if response.command == CMD_BUSY:
                    time.sleep(0.05)
                    continue

                raise BootloaderCommandError(
                    command=command,
                    response=response.command,
                    payload=response.payload,
                )

            except (
                BootloaderTimeout,
                BootloaderProtocolError,
            ) as exc:

                last_exception = exc

                if attempt + 1 < retries:
                    continue

                raise

        if last_exception is not None:
            raise last_exception

        raise BootloaderProtocolError(
            "Transaction failed"
        )

    # -----------------------------------------------------------------------
    # High-level commands
    # -----------------------------------------------------------------------

    def connect(self) -> Packet:
        """
        CONNECT must use sequence 0.

        The firmware resets the protocol session and therefore the host
        resets its sequence counter before sending CONNECT.
        """

        self.sequence = 0

        response = self.transact(
            CMD_CONNECT,
            b"",
        )

        return response

    def disconnect(self) -> Packet:
        return self.transact(
            CMD_DISCONNECT,
            b"",
        )

    def get_version(self) -> tuple[int, int, int]:
        response = self.transact(
            CMD_GET_VERSION,
            b"",
        )

        if len(response.payload) != 12:
            raise BootloaderProtocolError(
                "Invalid GET_VERSION response"
            )

        major, minor, patch = struct.unpack(
            "<3I",
            response.payload,
        )

        return (
            major,
            minor,
            patch,
        )

    def get_info(self) -> dict[str, int]:
        response = self.transact(
            CMD_GET_INFO,
            b"",
        )

        if len(response.payload) != 20:
            raise BootloaderProtocolError(
                "Invalid GET_INFO response"
            )

        (
            flash_start,
            flash_end,
            app_start,
            app_end,
            app_max_size,
        ) = struct.unpack(
            "<5I",
            response.payload,
        )

        return {
            "flash_start": flash_start,
            "flash_end": flash_end,
            "app_start": app_start,
            "app_end": app_end,
            "app_max_size": app_max_size,
        }

    def erase(
        self,
        address: int,
        size: int,
    ) -> Packet:

        payload = struct.pack(
            "<2I",
            address,
            size,
        )

        return self.transact(
            CMD_ERASE,
            payload,
        )

    def write(
        self,
        address: int,
        data: bytes,
    ) -> Packet:

        if len(data) == 0:
            raise ValueError(
                "Cannot write empty data"
            )

        if len(data) > 232:
            raise ValueError(
                "Write data exceeds one protocol packet"
            )

        payload = (
            struct.pack(
                "<2I",
                address,
                len(data),
            )
            + data
        )

        return self.transact(
            CMD_WRITE,
            payload,
        )

    def verify(self) -> Packet:
        return self.transact(
            CMD_VERIFY,
            b"",
        )

    def abort(self) -> Packet:
        return self.transact(
            CMD_ABORT,
            b"",
        )

    def run_application(self) -> Packet:
        return self.transact(
            CMD_RUN_APP,
            b"",
        )

    def reset(self) -> Packet:
        return self.transact(
            CMD_RESET,
            b"",
        )