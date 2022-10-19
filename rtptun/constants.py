from dataclasses import dataclass


@dataclass
class Constants:
    UDP_BUFFER_SIZE: int = 65507
    TIMEOUT: int = 120
    TIMESTAMP_INCREMENT = 3000  # 90kHz / 30FPS video
    # While the actual RTP payload dynamic range is 96 to 100 the safe range (at least here) seems to be 96 to 98
    DYNAMIC_RANGE = (96, 98)

    UINT16_MAX: int = (2 ** 16) - 1
    UINT32_MAX: int = (2 ** 32) - 1
