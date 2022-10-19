from dataclasses import dataclass


@dataclass
class Constants:
    UDP_BUFFER_SIZE: int = 65507
    TIMEOUT: int = 120

    UINT16_MAX: int = (2 ** 16) - 1
