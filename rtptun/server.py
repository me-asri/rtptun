import logging
import socket
import selectors

import xor

from rtp import RTPHeader


class RTPTunServer:
    BUFFER_SIZE = 65507

    def __init__(self, source_ip: str, source_port: int, dest_ip: str, dest_port: int, key: str = None) -> None:
        assert self.BUFFER_SIZE > RTPHeader.SIZE

        self.src_addr = (source_ip, source_port)
        self.dest_addr = (dest_ip, dest_port)

        self.key = key

        self.ssock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.ssock.bind(self.src_addr)

        self.buffer = bytearray(self.BUFFER_SIZE)

        self.rtp_hdr = RTPHeader.from_buffer(self.buffer)
        self.rtp_hdr.version = 2

        self.buffer_view = memoryview(self.buffer)

        self.sel = selectors.DefaultSelector()
        self.sel.register(self.ssock, selectors.EVENT_READ,
                          self.__ssock_callback)

        self.ssrc_map = {}

    def __ssock_callback(self, con: socket.socket, mask: int) -> None:
        try:
            data_len, addr = con.recvfrom_into(self.buffer)
        except:
            # Source refused connection
            logging.warning('Source refused connection')
            # TODO How do we cleanup data here?
            return

        # XOR payload if key is specified
        if self.key:
            xor.xor(
                self.buffer_view[RTPHeader.SIZE:data_len], self.key)

        ssrc = socket.ntohl(self.rtp_hdr.ssrc)

        # TODO Seperate SSRCs based on peer address
        if not ssrc in self.ssrc_map:
            dsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            dsock.bind(('0.0.0.0', 0))
            self.sel.register(dsock, selectors.EVENT_READ,
                              self.__dsock_callback)

            self.ssrc_map[ssrc] = (dsock, addr)

        dsock = self.ssrc_map[ssrc][0]
        dsock.sendto(
            self.buffer_view[RTPHeader.SIZE:data_len], self.dest_addr)

    def __dsock_callback(self, con: socket.socket, mask: int) -> None:
        try:
            ssrc = next(ssrc for ssrc, val in self.ssrc_map.items()
                        if val[0] == con)
        except StopIteration:
            # SSRC doesn't exist on remote side
            logging.warning(
                f'Failed to find SSRC for socket #{con.fileno}')
            return

        try:
            data_len = con.recv_into(
                self.buffer_view[RTPHeader.SIZE:])
        except ConnectionResetError:
            # Destination refused connection
            logging.warning('Destination refused connection')

            self.sel.unregister(con)
            con.close()

            del self.ssrc_map[ssrc]
            return

        new_len = RTPHeader.SIZE + data_len

        if self.key:
            xor.xor(
                self.buffer_view[RTPHeader.SIZE:new_len], self.key)

        self.rtp_hdr.ssrc = socket.htonl(ssrc)

        self.ssock.sendto(self.buffer_view[:new_len], self.ssrc_map[ssrc][1])

    def run(self):
        logging.info(
            f'Forwarding UDP traffic from {self.src_addr[0]}:{self.src_addr[1]} to {self.dest_addr[0]}:{self.dest_addr[1]}')

        while True:
            try:
                events = self.sel.select(1)
            except KeyboardInterrupt:
                logging.info('Bye bye!')
                return

            for key, mask in events:
                callback = key.data
                callback(key.fileobj, mask)
