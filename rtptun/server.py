from typing import Dict, Tuple
from dataclasses import dataclass

from rtptun.protocol.udp import SocketClosedError, UdpSocket
from rtptun.protocol.rtp import RtpHeader
from rtptun.constants import Constants
from rtptun.crypto import xor

import asyncio
import random
import socket
import logging


@dataclass
class _SubSocketInfo:
    sock: UdpSocket
    active: bool = True


@dataclass
class _MainSocketInfo:
    seq_num: int
    dest_sockets: Dict[int, _SubSocketInfo] = None


class RtptunServer:
    def __init__(self, source_ip: str, source_port: int, dest_ip: str, dest_port: int, key: str = None) -> None:
        assert Constants.UDP_BUFFER_SIZE > RtpHeader.SIZE

        self.src_addr = (source_ip, source_port)
        self.dest_addr = (dest_ip, dest_port)
        self._key = key

        self._src_sock: UdpSocket = None

        self._buffer = bytearray(Constants.UDP_BUFFER_SIZE)
        self._buffer_view = memoryview(self._buffer)

        self._rtp_hdr = RtpHeader.from_buffer(self._buffer)
        self._rtp_hdr.version = 2

        self._socket_map: Dict[Tuple[str, int], _MainSocketInfo] = {}

    async def __handle_remote_socket(self, sock: UdpSocket, peer_addr: Tuple[str, int], ssrc: int) -> None:
        while True:
            try:
                data_len, _ = await sock.recvfrom_into(self._buffer_view[RtpHeader.SIZE:])
            except SocketClosedError:
                return

            info = self._socket_map[peer_addr]
            sub_info = info.dest_sockets[ssrc]

            self._rtp_hdr.ssrc = socket.htonl(ssrc)
            self._rtp_hdr.seq_number = socket.htons(info.seq_num)

            # Increment sequence number for next packet
            info.seq_num += 1
            # Mark socket as active
            sub_info.active = True

            new_len = RtpHeader.SIZE + data_len

            if self._key:
                xor.xor(
                    self._buffer_view[RtpHeader.SIZE:new_len], self._key)

            await self._src_sock.sendto(
                self._buffer_view[:new_len], peer_addr)

    async def __handle_source_socket(self) -> None:
        while True:
            data_len, addr = await self._src_sock.recvfrom_into(self._buffer)

            if data_len < RtpHeader.SIZE:
                logging.warning('Packet with invalid size received')
                continue

            ssrc = socket.ntohl(self._rtp_hdr.ssrc)

            if not addr in self._socket_map:
                self._socket_map[addr] = _MainSocketInfo(
                    seq_num=random.getrandbits(16),
                    dest_sockets={}
                )

            info = self._socket_map[addr]
            if not ssrc in info.dest_sockets:
                sock = await UdpSocket.connect(
                    self.dest_addr)
                asyncio.create_task(
                    self.__handle_remote_socket(sock, addr, ssrc))

                info.dest_sockets[ssrc] = _SubSocketInfo(sock)

            # XOR payload if key is specified
            if self._key:
                xor.xor(
                    self._buffer_view[RtpHeader.SIZE:data_len], self._key)

            dest_sock = info.dest_sockets[ssrc].sock
            await dest_sock.sendto(
                self._buffer_view[RtpHeader.SIZE:data_len], self.dest_addr)

    async def __cleanup(self) -> None:
        while True:
            await asyncio.sleep(Constants.TIMEOUT)

            inactive_main = []
            inactive_sub = []

            for addr, info in self._socket_map.items():
                main_active = False

                for ssrc, sub_info in info.dest_sockets.items():
                    if sub_info.active:
                        sub_info.active = False
                        main_active = True
                    else:
                        inactive_sub.append((addr, ssrc))

                if not main_active:
                    inactive_main.append(addr)

            for addr, ssrc in inactive_sub:
                self._socket_map[addr].dest_sockets[ssrc].sock.close()
                del self._socket_map[addr].dest_sockets[ssrc]

            for addr in inactive_main:
                del self._socket_map[addr]

    async def run(self) -> None:
        logging.info(
            f'Forwarding UDP traffic from {self.src_addr[0]}:{self.src_addr[1]} to {self.dest_addr[0]}:{self.dest_addr[1]}')

        self._src_sock = await UdpSocket.new(self.src_addr)

        await asyncio.gather(
            self.__cleanup(),
            self.__handle_source_socket()
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

    parser = argparse.ArgumentParser()

    parser.add_argument('-x', '--xor',
                        help='XOR payload bytes with key (NOT A REPLACEMENT FOR PROPER ENCRYPTION!)', dest='key')
    parser.add_argument('-s', '--source-addr', default='0.0.0.0',
                        help='Source address for incoming connection [0.0.0.0]')
    parser.add_argument('-p', '--source-port', required=True,
                        help='Source port for incoming connection')
    parser.add_argument('-d', '--dest-addr', default='127.0.0.1',
                        help='Destination address for outgoing connection [127.0.0.1]')
    parser.add_argument('-q', '--dest-port', required=True,
                        help='Destination port for outgoing connection')

    args = vars(parser.parse_args())

    server = RtptunServer(args['source_addr'], int(args['source_port']),
                          args['dest_addr'], int(args['dest_port']), args['key'])
    try:
        asyncio.run(server.run())
    except KeyboardInterrupt:
        logging.info('Bye bye!')
