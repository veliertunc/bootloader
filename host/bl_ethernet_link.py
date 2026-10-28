from __future__ import annotations

import socket


class EthernetUDPLink:
    """Serial-like adapter for bootloader packets sent over W5500 UDP."""

    def __init__(self, host: str, port: int, timeout: float):
        self.peer = (host, port)
        self.timeout = timeout
        self.socket: socket.socket | None = None
        self.pending = bytearray()

    def open(self) -> None:
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.settimeout(self.timeout)
        self.socket.bind(("0.0.0.0", 0))
        self.pending.clear()

    def close(self) -> None:
        if self.socket is not None:
            self.socket.close()
            self.socket = None

    def write(self, data: bytes) -> int:
        if self.socket is None:
            raise OSError("Ethernet transport is not open")
        return self.socket.sendto(data, self.peer)

    def flush(self) -> None:
        return

    def read(self, length: int) -> bytes:
        if self.socket is None:
            raise OSError("Ethernet transport is not open")
        while not self.pending:
            try:
                data, _ = self.socket.recvfrom(2048)
            except socket.timeout:
                return b""
            if data:
                self.pending.extend(data)
        result = bytes(self.pending[:length])
        del self.pending[:length]
        return result

    def reset_input_buffer(self) -> None:
        if self.socket is None:
            return
        self.pending.clear()
        self.socket.setblocking(False)
        try:
            while True:
                self.socket.recvfrom(2048)
        except BlockingIOError:
            pass
        finally:
            self.socket.setblocking(True)
            self.socket.settimeout(self.timeout)

    def reset_output_buffer(self) -> None:
        return
