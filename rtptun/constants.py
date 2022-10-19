from dataclasses import dataclass


@dataclass
class Constants:
    UDP_BUFFER_SIZE: int = 65507
    TIMEOUT: int = 120
    TIMESTAMP_INCREMENT = 3000  # 90kHz / 30FPS video

    UINT16_MAX: int = (2 ** 16) - 1
    UINT32_MAX: int = (2 ** 32) - 1
