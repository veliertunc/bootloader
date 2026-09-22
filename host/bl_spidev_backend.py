from bl_spi_backend import SPIBackend

import spidev


class LinuxSPIBackend(SPIBackend):

    def __init__(
            self,
            bus: int = 0,
            device: int = 0,
            speed_hz: int = 1000000):

        self.bus = bus
        self.device = device
        self.speed_hz = speed_hz

        self.spi = None
    
    def open(self):

        self.spi = spidev.SpiDev()

        self.spi.open(
            self.bus,
            self.device)

        self.spi.max_speed_hz = \
            self.speed_hz

        self.spi.mode = 0
    
    def close(self):

        if self.spi:
            self.spi.close()
    
    def transfer(
            self,
            tx_data: bytes) -> bytes:

        response = self.spi.xfer2(
            list(tx_data))

        return bytes(response)