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

        self.ssrc_map = {}
        self.sock_map = {}

    def __ssock_callback(self, con: socket.socket, mask: int) -> None:
        try:
            data_len, addr = con.recvfrom_into(self.buffer)
        except:
            # TODO CLEAN UP!!!
            return

        self.rtp_hdr.deserialize(self.buffer)
        ssrc = socket.ntohl(self.rtp_hdr.ssrc)

        if not ssrc in self.ssrc_map:
            dsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            dsock.bind(('0.0.0.0', 0))
            self.sel.register(dsock, selectors.EVENT_READ,
                              self.__dsock_callback)

            self.ssrc_map[ssrc] = (dsock, addr)

        dsock = self.ssrc_map[ssrc][0]
        dsock.sendto(
            self.buffer_view[RTPHeader.RTP_HEADER_LEN:data_len], self.dest_addr)

    def __dsock_callback(self, con: socket.socket, mask: int) -> None:
        for ssrc, info in self.ssrc_map.items():
            if info[0] != con:
                continue

            addr = info[1]

        try:
            data_len = con.recv_into(
                self.buffer_view[RTPHeader.RTP_HEADER_LEN:])
        except ConnectionResetError:
            self.sel.unregister(con)
            con.close()

            del self.ssrc_map[ssrc]
            return

        self.rtp_hdr.ssrc = ssrc
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
