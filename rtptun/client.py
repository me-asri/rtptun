from typing import Dict

from rtptun.protocol.rtp import RtpSocket, RtpStream
from rtptun.protocol.udp import Address, UdpSocket
from rtptun.crypto import key

import asyncio
import logging


class Client:
    def __init__(self, local_port: int, remote_address: Address, key: bytes) -> None:
        self.local_port = local_port
        self.remote_address = remote_address
        self.key = key

        self._stream_map: Dict[Address, RtpStream] = {}

    async def __handle_stream(self, stream: RtpStream, udp: UdpSocket, dest_addr: Address) -> None:
        while True:
            data = await stream.recv()
            await udp.sendto(data, dest_addr)

    async def __handle_udp(self, udp: UdpSocket, rtp: RtpSocket) -> None:
        while True:
            addr, data = await udp.recvfrom()
            if not addr in self._stream_map:
                stream = rtp.create_stream()
                self._stream_map[addr] = stream
                asyncio.create_task(self.__handle_stream(stream, udp, addr))
            stream = self._stream_map[addr]

            await stream.send(data)

    async def run(self) -> None:
        rtp_sock = await RtpSocket.connect(self.remote_address, self.key)
        udp_sock = await UdpSocket.bind(('127.0.0.1', self.local_port))

        await self.__handle_udp(udp_sock, rtp_sock)


if __name__ == '__main__':
    import argparse
    import sys

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('-l', '--local-port', required=True, default=argparse.SUPPRESS,
                        help='local port for clients')
    parser.add_argument('-s', '--server-addr', required=True, default=argparse.SUPPRESS,
                        help='remote rtptun server address')
    parser.add_argument('-p', '--server-port', required=True, default=argparse.SUPPRESS,
                        help='remote rtptun server port')
    parser.add_argument('-k', '--key', required=True, default=argparse.SUPPRESS,
                        help='encryption key')

    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)
    logging.captureWarnings(True)

    try:
        parsed_key = key.parse_str_key(args.key)
    except ValueError:
        logging.error('Invalid key')
        exit(1)

    # Workaround for Proactor event loop bug on Windows
    # See: https://github.com/python/cpython/issues/91227
    if sys.platform.startswith('win32'):
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

    # Use uvloop if available
    try:
        import uvloop  # type: ignore

        uvloop.install()
        logging.info('Using uvloop for better performance')
    except ModuleNotFoundError:
        pass

    logging.info(
        f'Tunneling traffic from local port {args.local_port} to {args.server_addr}:{args.server_port}')

    client = Client(args.local_port, (args.server_addr,
                    args.server_port), parsed_key)
    try:
        asyncio.run(client.run())
    except KeyboardInterrupt:
        logging.info('Bye bye!')
