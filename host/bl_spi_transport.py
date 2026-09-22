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

    def receive(self) -> bytes:

        length_request = \
            bytes([0xFF] * 8)

        header = \
            self.backend.transfer(
                length_request)

        if len(header) < 8:
            raise RuntimeError(
                "Short header")

        payload_length = (
            header[4] |
            (header[5] << 8) |
            (header[6] << 16) |
            (header[7] << 24)
        )

        total_length = (
            8 +
            payload_length +
            4
        )

        response = \
            self.backend.transfer(
                bytes([0xFF] *
                      total_length)
            )

        return decode_frame(
            response)