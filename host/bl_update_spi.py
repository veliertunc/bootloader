from bl_protocol import BootloaderProtocol

from bl_spi_transport import \
    SPITransport

from bl_spidev_backend import \
    LinuxSPIBackend

backend = LinuxSPIBackend(
    bus=0,
    device=0,
    speed_hz=1000000)

transport = SPITransport(
    backend)

transport.open()

protocol = BootloaderProtocol(
    transport)

protocol.connect()

protocol.get_info()

protocol.erase()

protocol.write_file(
    "application.bin")

protocol.verify()

protocol.run_application()

transport.close()