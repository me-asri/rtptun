import logging
import socket
import selectors

import xor

from rtp import RTPHeader


class RTPTunServer:
    BUFFER_SIZE = 65507

    def __init__(self, source_ip: str, source_port: int, dest_ip: str, dest_port: int, key: str = None) -> None:
        self.src_addr = (source_ip, source_port)
        self.dest_addr = (dest_ip, dest_port)

        self.key = key

        self.ssock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.ssock.bind(self.src_addr)

        self.rtp_hdr = RTPHeader()

        self.buffer = bytearray(RTPHeader.RTP_HEADER_LEN + self.BUFFER_SIZE)
        self.rtp_hdr.serialize(self.buffer)
        self.buffer_view = memoryview(self.buffer)

        self.sel = selectors.DefaultSelector()
        self.sel.register(self.ssock, selectors.EVENT_READ,
                          self.__ssock_callback)

        self.addr_map = {}
        self.ssrc_map = {}
        self.sock_map = {}

    def __ssock_callback(self, con: socket.socket, mask: int) -> None:
        try:
            data_len, addr = con.recvfrom_into(self.buffer)
        except:
            # TODO clean up?
            return

        # XOR payload if key is specified
        if self.key:
            xor.xor(
                self.buffer_view[RTPHeader.RTP_HEADER_LEN:data_len], self.key)

        self.rtp_hdr.deserialize(self.buffer)
        ssrc = socket.ntohl(self.rtp_hdr.ssrc)

        if not addr in self.ssrc_map:
            dsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            dsock.bind(('0.0.0.0', 0))
            self.sel.register(dsock, selectors.EVENT_READ,
                              self.__dsock_callback)

            self.addr_map[dsock] = addr
            self.ssrc_map[addr] = ssrc
            self.sock_map[ssrc] = dsock

        dsock = self.sock_map[ssrc]
        dsock.sendto(
            self.buffer_view[RTPHeader.RTP_HEADER_LEN:data_len], self.dest_addr)

    def __dsock_callback(self, con: socket.socket, mask: int) -> None:
        addr = self.addr_map[con]
        ssrc = self.ssrc_map[addr]

        try:
            data_len = con.recv_into(
                self.buffer_view[RTPHeader.RTP_HEADER_LEN:])
        except ConnectionResetError:
            self.sel.unregister(con)

            del self.addr_map[con]
            del self.ssrc_map[addr]
            del self.sock_map[ssrc]

            con.close()
            return

        new_len = RTPHeader.RTP_HEADER_LEN + data_len

        if self.key:
            xor.xor(
                self.buffer_view[RTPHeader.RTP_HEADER_LEN:new_len], self.key)

        self.rtp_hdr.ssrc = socket.htonl(ssrc)
        self.rtp_hdr.serialize(self.buffer)

        self.ssock.sendto(self.buffer_view[:new_len], addr)

    def run(self):
        logging.info(
            f'Forwarding UDP traffic from {self.src_addr[0]}:{self.src_addr[1]} to {self.dest_addr[0]}:{self.dest_addr[1]}')

        while True:
            try:
                events = self.sel.select(0.5)
            except KeyboardInterrupt:
                logging.info('Bye bye!')
                return

            for key, mask in events:
                callback = key.data
                callback(key.fileobj, mask)
