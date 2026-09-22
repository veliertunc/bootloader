from abc import ABC
from abc import abstractmethod


class SPIBackend(ABC):

    @abstractmethod
    def open(self):
        pass

    @abstractmethod
    def close(self):
        pass

    @abstractmethod
    def transfer(self, tx_data: bytes) -> bytes:
        pass