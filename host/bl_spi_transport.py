import time

from bl_transport import BootloaderTransport

from bl_spi_framing import (
    encode_frame,
    decode_frame
)

from bl_spi_backend import SPIBackend

class SPITransport(
        BootloaderTransport):

    def __init__(
            self,
            backend: SPIBackend):

        self.backend = backend

    def open(self):

        self.backend.open()

    def close(self):

        self.backend.close()
        
    def send(
            self,
            data: bytes):

        frame = encode_frame(
            data)

        self.backend.transfer(
            frame)

        # The target is a slave and cannot tell the host when its
        # command handler has finished. Leave the bus idle before polling.
        if len(data) >= 14 and data[2] == 0x10:
            erase_size = int.from_bytes(data[10:14], "little")
            time.sleep(max(0.02, (erase_size // 1024) * 0.025))
        elif len(data) >= 6 and data[2] == 0x12:
            time.sleep(0.05)
        else:
            time.sleep(0.01)

    def receive(self) -> bytes:
        header = b""
        for _ in range(50):
            header = self.backend.transfer(bytes([0xFF] * 8))
            if len(header) >= 8 and header[:4] == bytes([0x55, 0xAA, 0x55, 0xAA]):
                break
            time.sleep(0.01)
        else:
            raise TimeoutError("No SPI response frame became ready")

        payload_length = (
            header[4] |
            (header[5] << 8) |
            (header[6] << 16) |
            (header[7] << 24)
        )

        if payload_length > 250:
            raise ValueError("SPI response frame declares an invalid packet length")

        tail_length = payload_length + 4
        tail = self.backend.transfer(bytes([0xFF] * tail_length))

        return decode_frame(header + tail)
