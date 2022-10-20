from typing import Tuple, Union

import asyncio
import warnings

Address = Tuple[str, int]


class UdpProtocol(asyncio.DatagramProtocol):
    def __init__(self, socket: 'UdpSocket'):
        self.writing = asyncio.Event()
        self.writing.set()

        self._socket = socket

    # Protocol callbacks

    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        self._transport = transport

    def connection_lost(self, exc: Union[Exception, None]) -> None:
        self.writing.set()
        self._socket._store_datagram(None, None)

        self._transport.close()

    # Flow control callbacks #

    def pause_writing(self) -> None:
        self.writing.clear()

    def resume_writing(self) -> None:
        self.writing.set()

    # Datagram callbacks #

    def datagram_received(self, data, addr) -> None:
        self._socket._store_datagram(addr, data)

    def error_received(self, exc: Exception) -> None:
        warnings.warn(str(exc))


class UdpSocket:
    def __init__(self):
        self._closed = False
        self._connected = False

        self._queue = asyncio.Queue()

        self._protocol: UdpProtocol = None
        self._transport: asyncio.DatagramTransport = None

    @staticmethod
    async def bind(listen_address: Address = None) -> 'UdpSocket':
        if not listen_address:
            listen_address = ('0.0.0.0', 0)

        self = UdpSocket()

        loop = asyncio.get_running_loop()

        self._transport, self._protocol = await loop.create_datagram_endpoint(
            lambda: UdpProtocol(self),
            local_addr=listen_address
        )

        return self

    @staticmethod
    async def connect(remote_address: Address, local_address: Union[Address, None] = None) -> 'UdpSocket':
        self = UdpSocket()
        self._connected = True

        loop = asyncio.get_running_loop()
        self._transport, self._protocol = await loop.create_datagram_endpoint(
            lambda: UdpProtocol(self),
            local_addr=local_address,
            remote_addr=remote_address
        )

        return self

    def _store_datagram(self, address: Address, datagram: bytes) -> None:
        try:
            self._queue.put_nowait((address, datagram))
        except asyncio.QueueFull:
            warnings.warn(
                'Socket queue is full')

    async def recvfrom(self) -> Tuple[Address, bytes]:
        if self._closed and self._queue.empty():
            raise SocketClosedError('Socket closed')

        addr, data = await self._queue.get()
        if not addr:
            self._closed = True
            raise SocketClosedError('Socket closed')

        return (addr, data)

    async def sendto(self, data: bytes, address: Address) -> int:
        if self._closed:
            raise SocketClosedError('Socket closed')

        # Wait until event loop write buffer is drained
        await self._protocol.writing.wait()

        self._transport.sendto(data, address)
        return len(data)

    async def send(self, data: bytes) -> int:
        if not self._connected:
            raise SocketNotConnectedError('Socket not connected')

        return await self.sendto(data, None)

    def close(self):
        if self._closed:
            return
        self._closed = True

        self._transport.close()

    @property
    def closed(self):
        return self._closed

    @property
    def connected(self):
        return self._connected

    @property
    def local_address(self):
        sock = self._transport.get_extra_info('socket')
        return sock.getsockname()

    @property
    def remote_address(self):
        if not self._connected:
            raise SocketNotConnectedError('Socket not connected')

        sock = self._transport.get_extra_info('socket')
        return sock.getpeername()


class SocketClosedError(IOError):
    def __init__(self, *args):
        Exception.__init__(self, *args)


class SocketNotConnectedError(IOError):
    def __init__(self, *args):
        Exception.__init__(self, *args)
