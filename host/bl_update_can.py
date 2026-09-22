from bl_can_transport import CanTransport
from bl_protocol import BootloaderProtocol

transport = CanTransport()

transport.open()

protocol = BootloaderProtocol(
    transport)

protocol.connect()

protocol.erase()

protocol.write_file(
    "application.bin")

protocol.verify()

protocol.run_application()

transport.close()