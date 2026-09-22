from abc import ABC
from abc import abstractmethod

import queue
import threading

import can

from bl_transport_can import BootloaderTransport

CAN_HOST_TO_TARGET = 0x321
CAN_TARGET_TO_HOST = 0x322

FIRST = 0x01
LAST = 0x02

FRAGMENT_SIZE = 5

class BootloaderTransport(ABC):

    @abstractmethod
    def open(self):
        pass

    @abstractmethod
    def close(self):
        pass

    @abstractmethod
    def send(self, data: bytes):
        pass

    @abstractmethod
    def receive(self) -> bytes:
        pass

class CanTransport(BootloaderTransport):

    def __init__(
            self,
            channel="PCAN_USBBUS1",
            bitrate=500000):

        self.channel = channel
        self.bitrate = bitrate

        self.bus = None

        self.rx_queue = queue.Queue()

        self.transfer_id = 1

        self.listener_thread = None

        self.running = False
    
    def open(self):

        self.bus = can.interface.Bus(
            interface="pcan",
            channel=self.channel,
            bitrate=self.bitrate)

        self.running = True

        self.listener_thread = threading.Thread(
            target=self._listener)

        self.listener_thread.start()
    
    def close(self):

        self.running = False

        if self.listener_thread:
            self.listener_thread.join()

        if self.bus:
            self.bus.shutdown()
            
    def _listener(self):

        while self.running:

            msg = self.bus.recv(1.0)

            if msg is None:
                continue

            if msg.arbitration_id != \
                    CAN_TARGET_TO_HOST:
                continue

            self.rx_queue.put(msg)

    def send(self, data: bytes):

        offset = 0
        fragment = 0

        transfer_id = self.transfer_id

        self.transfer_id += 1

        while offset < len(data):

            chunk = data[
                offset:
                offset + FRAGMENT_SIZE
            ]

            flags = 0

            if offset == 0:
                flags |= FIRST

            if offset + len(chunk) >= len(data):
                flags |= LAST

            payload = bytes([
                transfer_id,
                fragment,
                flags
            ]) + chunk.ljust(
                FRAGMENT_SIZE,
                b'\x00')

            msg = can.Message(
                arbitration_id=
                CAN_HOST_TO_TARGET,
                data=payload,
                is_extended_id=False)

            self.bus.send(msg)

            offset += len(chunk)
            fragment += 1

    def receive(self):

        packet = bytearray()

        expected_transfer = None

        while True:

            msg = self.rx_queue.get()

            transfer_id = msg.data[0]
            flags = msg.data[2]

            if expected_transfer is None:
                expected_transfer = transfer_id

            if transfer_id != expected_transfer:
                continue

            packet.extend(msg.data[3:8])

            if flags & LAST:
                return bytes(packet)