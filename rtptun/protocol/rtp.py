import ctypes as ct


class RtpHeader(ct.BigEndianStructure):
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


RtpHeader.SIZE = ct.sizeof(RtpHeader)
