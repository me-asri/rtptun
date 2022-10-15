import socket
import selectors
import random

from rtp import RTPHeader


class RTPTunClient:
    BUFFER_SIZE = 8192

    def __init__(self, local_port: int, remote_ip: str, remote_port: int) -> None:
        self.remote_addr = (remote_ip, remote_port)

        self.lsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.lsock.bind(('127.0.0.1', local_port))

        self.rsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.rsock.bind(('0.0.0.0', 0))

        self.rtp_hdr = RTPHeader()

        self.buffer = bytearray(RTPHeader.RTP_HEADER_LEN + self.BUFFER_SIZE)
        self.rtp_hdr.serialize(self.buffer)
        self.buffer_view = memoryview(self.buffer)

        self.sel = selectors.DefaultSelector()
        self.sel.register(self.lsock, selectors.EVENT_READ,
                          self.__lsock_callback)
        self.sel.register(self.rsock, selectors.EVENT_READ,
                          self.__rsock_callback)

        self.ssrc_map = {}
        self.addr_map = {}

    def __lsock_callback(self, con: socket.socket, mask: int) -> None:
        data_len, recv_addr = con.recvfrom_into(
            self.buffer_view[RTPHeader.RTP_HEADER_LEN:])

        if not recv_addr in self.ssrc_map:
            ssrc = random.getrandbits(32)
            self.ssrc_map[recv_addr] = ssrc
            self.addr_map[ssrc] = recv_addr

        # Using SSRC field as local UDP identifier
        self.rtp_hdr.ssrc = socket.htonl(self.ssrc_map[recv_addr])
        self.rtp_hdr.serialize(self.buffer)

        new_len = RTPHeader.RTP_HEADER_LEN + data_len
        self.rsock.sendto(self.buffer_view[:new_len], self.remote_addr)

    def __rsock_callback(self, con: socket.socket, mask: int) -> None:
        data_len = con.recv_into(self.buffer)

        self.rtp_hdr.deserialize(self.buffer)
        ssrc = socket.ntohl(self.rtp_hdr.ssrc)

        # addr = next(addr for addr, s in self.ssrc_map.items()
        #             if socket.ntohl(s) == ssrc)
        self.lsock.sendto(
            self.buffer_view[RTPHeader.RTP_HEADER_LEN:data_len], self.addr_map[ssrc])

    def run(self):
        while True:
            # It's impossible to close program without timeout
            events = self.sel.select(2)

            for key, mask in events:
                callback = key.data
                callback(key.fileobj, mask)
