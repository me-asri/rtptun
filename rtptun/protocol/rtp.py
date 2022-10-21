from typing import Dict

from rtptun.protocol.udp import SocketClosedError, UdpSocket, Address
from rtptun.crypto import chacha

import ctypes as ct
import asyncio
import socket
import random
import warnings
import logging
import concurrent.futures


_RTP_HEADER_SIZE = 12

_RTP_SSRC_BITS = 32

_RTP_TIMESTAMP_BITS = 32
_RTP_TIMESTAMP_MAX = (2 ** _RTP_TIMESTAMP_BITS)
_RTP_TIMESTAMP_INCREMENT = 3000  # 90kHz / 30FPS video

_RTP_SEQ_BITS = 16
_RTP_SEQ_MAX = (2 ** _RTP_SEQ_BITS)

_RTP_TYPE = 97  # Dynamic

_RTP_STREAM_QUEUE_SIZE = 32
_RTP_SOCKET_QUEUE_SIZE = 8


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


class RtpStream:
    def __init__(self, ssrc: int, master: 'RtpSocket', type: int = None, address: Address = None):
        self.ssrc = ssrc
        self.master = master
        self.timestamp = random.getrandbits(_RTP_TIMESTAMP_BITS)
        self.type = type

        self.address = address

        self._data_queue = asyncio.Queue(_RTP_STREAM_QUEUE_SIZE)

    def _put_data(self, data: bytes):
        try:
            self._data_queue.put_nowait(data)
        except asyncio.QueueFull:
            warnings.warn('RtpStream data queue full')
            return

    async def send(self, data: bytes):
        await self.master._send(self, data)

        self.timestamp = (
            self.timestamp + _RTP_TIMESTAMP_INCREMENT) % _RTP_TIMESTAMP_MAX

    async def recv(self) -> bytes:
        return await self._data_queue.get()


class RtpSocket:
    def __init__(self, key: bytes) -> None:
        self.seq_number = random.getrandbits(_RTP_SEQ_BITS)
        self.key = key

        self._socket: UdpSocket = None

        self._ssrc_map: Dict[int, RtpStream] = {}
        self._stream_queue = asyncio.Queue(_RTP_SOCKET_QUEUE_SIZE)

        self._loop = asyncio.get_running_loop()
        self._pool = concurrent.futures.ProcessPoolExecutor()

    @staticmethod
    async def connect(address: Address, key: bytes) -> 'RtpSocket':
        self = RtpSocket(key)

        self._socket = await UdpSocket.connect(address)
        asyncio.create_task(self._recv())

        return self

    @staticmethod
    async def listen(key: bytes, address: Address = ('0.0.0.0', 16384)) -> 'RtpSocket':
        self = RtpSocket(key)

        self._socket = await UdpSocket.bind(address)
        asyncio.create_task(self._recv())

        return self

    def create_stream(self) -> RtpStream:
        ssrc = random.getrandbits(_RTP_SSRC_BITS)
        while ssrc in self._ssrc_map:
            ssrc = random.getrandbits(_RTP_SSRC_BITS)
        type = _RTP_TYPE

        stream = RtpStream(ssrc, self, type)
        self._ssrc_map[ssrc] = stream
        return stream

    async def recv_stream(self) -> RtpStream:
        return await self._stream_queue.get()

    async def _send(self, stream: RtpStream, data: bytes) -> None:
        data_len = len(data)

        buffer = bytearray(_RTP_HEADER_SIZE + data_len +
                           chacha.CHACHA_OVERHEAD)
        payload = memoryview(buffer)[_RTP_HEADER_SIZE:]

        header = RtpHeader.from_buffer(buffer)
        header.version = 2
        header.ssrc = socket.htonl(stream.ssrc)
        header.seq_number = self.seq_number
        header.timestamp = stream.timestamp
        header.payload_type = stream.type

        cipher, nonce, tag = await self._loop.run_in_executor(self._pool, chacha.encrypt, data, self.key)
        payload[:data_len] = cipher

        nonce_end = data_len + chacha.CHACHA_NONCE_LEN
        payload[data_len:nonce_end] = nonce
        payload[nonce_end:nonce_end + chacha.CHACHA_TAG_LEN] = tag

        self.seq_number = (self.seq_number + 1) % _RTP_SEQ_MAX

        if stream.address:
            await self._socket.sendto(buffer, stream.address)
        else:
            await self._socket.send(buffer)

    async def _recv(self) -> None:
        while True:
            try:
                addr, data = await self._socket.recvfrom()
            except SocketClosedError:
                # Socket closed
                return

            # Drop packets with invalid payload size
            if len(data) <= _RTP_HEADER_SIZE + chacha.CHACHA_OVERHEAD:
                logging.warning(
                    f'Received packet with invalid size from {addr[0]}:{addr[1]}')
                continue

            header = RtpHeader.from_buffer_copy(data)
            ssrc = socket.ntohl(header.ssrc)
            type = header.payload_type

            payload_len = len(data) - chacha.CHACHA_OVERHEAD - _RTP_HEADER_SIZE

            nonce_start = _RTP_HEADER_SIZE + payload_len
            nonce_end = nonce_start + chacha.CHACHA_NONCE_LEN
            nonce = data[nonce_start:nonce_end]
            tag = data[nonce_end:]

            plain_data = await self._loop.run_in_executor(self._pool, chacha.decrypt,
                                                          data, payload_len, _RTP_HEADER_SIZE, self.key, nonce, tag)
            if not plain_data:
                logging.warning(
                    f'Failed to decrypt payload from {addr[0]}:{addr[1]}#{ssrc}')
                continue

            if not ssrc in self._ssrc_map:
                stream = RtpStream(ssrc, self, type, addr)
                try:
                    self._stream_queue.put_nowait(stream)
                except asyncio.QueueFull:
                    warnings.warn('RtpSocket stream queue full')
                    continue
                self._ssrc_map[ssrc] = stream

            stream = self._ssrc_map[ssrc]
            stream._put_data(plain_data)
