import ctypes as ct
import socket

from typing import Union


class RTPHeader(ct.BigEndianStructure):
    RTP_HEADER_LEN = 12

    _pack_ = 1
    _fields_ = [
        # RTP version (currently 2)
        ('version', ct.c_uint8, 2),
        # Indicates extra padding bytes at the end of the RTP packet
        ('padding', ct.c_uint8, 1),
        # Indicates presence of an extension header between the header and payload data
        ('extension', ct.c_uint8, 1),
        # Number of CSRC identifiers
        ('csrc_count', ct.c_uint8, 4),
        # Signaling used at the application level
        ('marker', ct.c_uint8, 1),
        # Indicates the format of the payload
        ('payload_type', ct.c_uint8, 7),
        # Sequence number (incremented for each RTP data packet)
        ('seq_number', ct.c_uint16),
        # Timestamp
        ('timestamp', ct.c_uint32),
        # Synchronization source identifier
        ('ssrc', ct.c_uint32)
    ]

    def __init__(self) -> None:
        self.version = 2  # Latest version
        self.marker = 1  # Using marker
        self.payload_type = 8  # G.711

    def __len__(self) -> int:
        return self.RTP_HEADER_LEN

    def serialize(self, buffer: Union[bytearray, memoryview]) -> None:
        hdr_len = ct.sizeof(self)
        buffer[:hdr_len] = bytes(self)

    def deserialize(self, buffer: Union[bytearray, memoryview]) -> None:
        ct.memmove(ct.pointer(self), bytes(
            buffer[:self.RTP_HEADER_LEN]), ct.sizeof(self))
