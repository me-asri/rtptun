import socket
import selectors

from rtp import RTPHeader


class RTPTunServer:
    BUFFER_SIZE = 8192

    def __init__(self, source_ip: str, source_port: int, dest_ip: str, dest_port: int) -> None:
        self.dest_addr = (dest_ip, dest_port)

        self.ssock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.ssock.bind((source_ip, source_port))

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

        self.rtp_hdr.ssrc = socket.htonl(ssrc)
        self.rtp_hdr.serialize(self.buffer)

        new_len = RTPHeader.RTP_HEADER_LEN + data_len
        self.ssock.sendto(self.buffer_view[:new_len], addr)

    def run(self):
        while True:
            # It's impossible to close program without timeout
            events = self.sel.select(2)

            for key, mask in events:
                callback = key.data
                callback(key.fileobj, mask)
