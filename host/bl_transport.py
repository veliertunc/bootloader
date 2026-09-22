from __future__ import annotations

from abc import ABC
from abc import abstractmethod


class BootloaderTransport(ABC):
    """
    Generic transport interface used by BootloaderProtocol.

    The protocol layer does not know whether the physical transport
    is UART, CAN, SPI, I2C, or USB HID.
    """

    @abstractmethod
    def open(self) -> None:
        """Open the physical transport."""
        raise NotImplementedError

    @abstractmethod
    def close(self) -> None:
        """Close the physical transport."""
        raise NotImplementedError

    @abstractmethod
    def send(self, data: bytes) -> None:
        """
        Send one complete bootloader packet.

        The transport implementation is responsible for any required
        fragmentation.
        """
        raise NotImplementedError

    @abstractmethod
    def receive(self) -> bytes:
        """
        Receive one complete bootloader packet.

        The transport implementation is responsible for reassembly.
        """
        raise NotImplementedError