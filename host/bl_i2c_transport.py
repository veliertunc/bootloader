from __future__ import annotations

import time

from bl_transport import BootloaderTransport


class I2CTransport(BootloaderTransport):
    """I2C master transport for the TM4C slave at address 0x42."""

    def __init__(self, bus: int = 1, address: int = 0x42, timeout: float = 2.0):
        self.bus_number = bus
        self.address = address
        self.timeout = timeout
        self.bus: SMBus | None = None
        self.i2c_message = None

    def open(self) -> None:
        from smbus2 import SMBus, i2c_msg

        self.bus = SMBus(self.bus_number)
        self.i2c_message = i2c_msg

    def close(self) -> None:
        if self.bus is not None:
            self.bus.close()
            self.bus = None

    def send(self, data: bytes) -> None:
        if self.bus is None:
            raise RuntimeError("I2C transport is not open")
        self.bus.i2c_rdwr(self.i2c_message.write(self.address, data))

    def receive(self) -> bytes:
        if self.bus is None:
            raise RuntimeError("I2C transport is not open")
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            message = self.i2c_message.read(self.address, 250)
            self.bus.i2c_rdwr(message)
            data = bytes(message)
            start = data.find(b"\x55\xAA")
            if start >= 0 and len(data) >= start + 6:
                payload_length = data[start + 4] | (data[start + 5] << 8)
                packet_length = 6 + payload_length + 4
                if payload_length <= 240 and len(data) >= start + packet_length:
                    return data[start : start + packet_length]
            time.sleep(0.01)
        raise TimeoutError("Timed out waiting for I2C bootloader response")
