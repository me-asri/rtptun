from typing import Dict, Tuple, Union
from dataclasses import dataclass

from rtptun.protocol.udp import Address, UdpSocket
from rtptun.protocol.rtp import RtpHeader
from rtptun.constants import Constants
from rtptun.crypto.xor import xor

import asyncio
import random
import socket
import logging


@dataclass
class _SocketInfo:
    ssrc: int
    active: bool = True


class RtptunClient:
    def __init__(self, local_port: int, remote_ip: str, remote_port: int, key: str = None) -> None:
        assert Constants.UDP_BUFFER_SIZE > RtpHeader.SIZE

        self._local_port = local_port
        self._remote_addr = (remote_ip, remote_port)
        self._key = key

        self._local_sock: UdpSocket = None
        self._remote_sock: UdpSocket = None

        self._buffer = bytearray(Constants.UDP_BUFFER_SIZE)
        self._buffer_view = memoryview(self._buffer)

        self.seq_num = random.getrandbits(16)

        self._rtp_hdr = RtpHeader.from_buffer(self._buffer)
        self._rtp_hdr.version = 2

        self._socket_map: Dict[Address, _SocketInfo] = {}

    def __get_socket_info(self, ssrc: int) -> Union[Tuple[Address, _SocketInfo], None]:
        try:
            addr, info = next((addr, info) for addr, info in self._socket_map.items()
                              if info.ssrc == ssrc)
        except StopIteration:
            return None

        return (addr, info)

    async def __handle_local_socket(self) -> None:
        while True:
            data_len, recv_addr = await self._local_sock.recvfrom_into(
                self._buffer_view[RtpHeader.SIZE:])

            new_len = RtpHeader.SIZE + data_len

            if not recv_addr in self._socket_map:
                ssrc = random.getrandbits(32)
                while self.__get_socket_info(ssrc):
                    ssrc = random.getrandbits(32)

                self._socket_map[recv_addr] = _SocketInfo(
                    ssrc=random.getrandbits(32))

            info = self._socket_map[recv_addr]
            # Using SSRC field as local UDP identifier
            self._rtp_hdr.ssrc = socket.htonl(info.ssrc)
            self._rtp_hdr.seq_number = socket.htons(self.seq_num)
            # Increment sequence number for next packet
            self.seq_num += 1

            # Mark socket as active
            info.active = True

            # XOR payload if key is specified
            if self._key:
                xor(self._buffer_view[RtpHeader.SIZE:new_len],
                    self._key, self._buffer_view[RtpHeader.SIZE:new_len])

            await self._remote_sock.send(self._buffer_view[:new_len])

    async def __handle_remote_socket(self) -> None:
        while True:
            data_len, _ = await self._remote_sock.recvfrom_into(self._buffer)

            if data_len < RtpHeader.SIZE:
                logging.warning('Packet with invalid size received')
                continue

            ssrc = socket.ntohl(self._rtp_hdr.ssrc)

            addr, info = self.__get_socket_info(ssrc) or (None, None)
            if not addr:
                logging.warning(
                    f'Failed to find local address for SSRC {ssrc}')
                continue

            if self._key:
                xor(self._buffer_view[RtpHeader.SIZE:data_len],
                    self._key, self._buffer_view[RtpHeader.SIZE:data_len])

            # Mark socket as active
            info.active = True

            await self._local_sock.sendto(
                self._buffer_view[RtpHeader.SIZE:data_len], addr)

    async def __cleanup(self) -> None:
        while True:
            await asyncio.sleep(Constants.TIMEOUT)

            inactive = []

            for addr, info in self._socket_map.items():
                if info.active:
                    info.active = False
                else:
                    inactive.append(addr)

            for addr in inactive:
                del self._socket_map[addr]

    async def run(self) -> None:
        logging.info(
            f'Tunneling UDP traffic from local port {self._local_port} to {self._remote_addr[0]}:{self._remote_addr[1]}')

        self._local_sock = await UdpSocket.new(('127.0.0.1', self._local_port))
        self._remote_sock = await UdpSocket.connect(self._remote_addr)

        await asyncio.gather(
            self.__handle_local_socket(),
            self.__handle_remote_socket(),
            self.__cleanup()
        )


if __name__ == '__main__':
    import argparse
    import sys

    logging.basicConfig(level=logging.INFO)
    logging.captureWarnings(True)

    if sys.platform.startswith('win32'):
        logging.warning(
            'Connections may fail after disconnection due to a bug in asyncio on Windows')
        logging.warning(
            'See https://github.com/python/cpython/issues/91227')

    # Use uvloop if available
    try:
        import uvloop  # type: ignore

        uvloop.install()
        logging.info('Using uvloop for better performance')
    except ModuleNotFoundError:
        pass

    parser = argparse.ArgumentParser()

    parser.add_argument('-x', '--xor',
                        help='XOR payload bytes with key (NOT A REPLACEMENT FOR PROPER ENCRYPTION!)', dest='key')
    parser.add_argument('-l', '--local-port', required=True,
                        help='Local port for incoming connection')
    parser.add_argument('-s', '--server-addr', required=True,
                        help='Server address')
    parser.add_argument('-p', '--server-port', required=True,
                        help='Server port')

    args = vars(parser.parse_args())

    client = RtptunClient(int(args['local_port']),
                          args['server_addr'], int(args['server_port']), args['key'])
    try:
        asyncio.run(client.run())
    except KeyboardInterrupt:
        logging.info('Bye bye!')
