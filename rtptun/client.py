import logging
import socket
import selectors
import random

import xor

from rtp import RTPHeader


class RTPTunClient:
    BUFFER_SIZE = 65507

    def __init__(self, local_port: int, remote_ip: str, remote_port: int, key: str = None) -> None:
        self.local_port = local_port
        self.remote_addr = (remote_ip, remote_port)
        self.key = key

        self.lsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.lsock.bind(('127.0.0.1', local_port))

        self.rsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.rsock.bind(('0.0.0.0', 0))

        self.buffer = bytearray(RTPHeader.SIZE + self.BUFFER_SIZE)

        self.rtp_hdr = RTPHeader.from_buffer(self.buffer)
        self.rtp_hdr.version = 2

        self.buffer_view = memoryview(self.buffer)

        self.sel = selectors.DefaultSelector()
        self.sel.register(self.lsock, selectors.EVENT_READ,
                          self.__lsock_callback)
        self.sel.register(self.rsock, selectors.EVENT_READ,
                          self.__rsock_callback)

        self.ssrc_map = {}

    def __lsock_callback(self, con: socket.socket, mask: int) -> None:
        try:
            data_len, recv_addr = con.recvfrom_into(
                self.buffer_view[RTPHeader.SIZE:])
        except ConnectionResetError:
            # Local client closed connection
            logging.debug('Local client refused connection')
            return

        new_len = RTPHeader.SIZE + data_len

        if not recv_addr in self.ssrc_map:
            self.ssrc_map[recv_addr] = random.getrandbits(32)

        # Using SSRC field as local UDP identifier
        self.rtp_hdr.ssrc = socket.htonl(self.ssrc_map[recv_addr])

        # XOR payload if key is specified
        if self.key:
            xor.xor(
                self.buffer_view[RTPHeader.SIZE:new_len], self.key)

        try:
            self.rsock.sendto(self.buffer_view[:new_len], self.remote_addr)
        except OSError:
            logging.error(
                'Failed to send datagram to remote server. No internet connection?')

            del self.ssrc_map[recv_addr]
            return

    def __rsock_callback(self, con: socket.socket, mask: int) -> None:
        try:
            data_len = con.recv_into(self.buffer)
        except ConnectionResetError:
            # RTPTun server disconnected
            logging.warning('Remote server refused connection')
            # TODO How do we cleanup data here?
            return

        ssrc = socket.ntohl(self.rtp_hdr.ssrc)

        try:
            addr = next(addr for addr, s in self.ssrc_map.items()
                        if s == ssrc)
        except StopIteration:
            # SSRC doesn't exist on client side
            logging.warning(f'Failed to find local address for SSRC {ssrc}')
            return

        if self.key:
            xor.xor(
                self.buffer_view[RTPHeader.SIZE:data_len], self.key)

        self.lsock.sendto(
            self.buffer_view[RTPHeader.SIZE:data_len], addr)

    def run(self):
        logging.info(
            f'Tunneling UDP traffic from local {self.local_port} port to {self.remote_addr[0]}:{self.remote_addr[1]}')

        while True:
            try:
                events = self.sel.select(0.5)
            except KeyboardInterrupt:
                logging.info('Bye bye!')
                return

            for key, mask in events:
                callback = key.data
                callback(key.fileobj, mask)
