from typing import Dict

from rtptun.protocol.udp import Address, UdpSocket
from rtptun.protocol.rtp import RtpSocket, RtpStream
from rtptun.crypto import key

import asyncio
import logging


class Server:
    def __init__(self, listen_address: Address, destination_address: Address, key: bytes) -> None:
        self.listen_address = listen_address
        self.destination_address = destination_address

        self.key = key

        self._udp_map: Dict[RtpStream, UdpSocket] = {}

    async def __handle_udp(self, udp: UdpSocket, stream: RtpStream) -> None:
        while True:
            _, data = await udp.recvfrom()
            await stream.send(data)

    async def __handle_stream(self, stream: RtpStream) -> None:
        while True:
            data = await stream.recv()

            if not stream in self._udp_map:
                sock = await UdpSocket.connect(self.destination_address)
                asyncio.create_task(self.__handle_udp(sock, stream))

                self._udp_map[stream] = sock

            sock = self._udp_map[stream]
            await sock.send(data)

    async def __handle_rtp(self, rtp: RtpSocket) -> None:
        while True:
            stream = await rtp.recv_stream()
            asyncio.create_task(self.__handle_stream(stream))

    async def run(self) -> None:
        self._rtp_sock = await RtpSocket.listen(self.key, self.listen_address)

        await self.__handle_rtp(self._rtp_sock)


if __name__ == '__main__':
    import argparse
    import sys

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    class genkey_action(argparse.Action):
        def __init__(self, option_strings, dest, nargs=0, **kwargs):
            return super().__init__(option_strings, dest, nargs, **kwargs)

        def __call__(self, parser, namespace, value, option_string, **kwargs):
            print(key.gen_str_key())
            parser.exit()

    parser.add_argument('--gen-key', action=genkey_action, required=False, default=argparse.SUPPRESS,
                        help='generate random key')

    parser.add_argument('-i', '--listen-addr', default='0.0.0.0',
                        help='listen address for incoming connection')
    parser.add_argument('-l', '--listen-port', required=True, default=argparse.SUPPRESS,
                        help='listen port for incoming connection')
    parser.add_argument('-d', '--dest-addr', default='127.0.0.1',
                        help='destination address for outgoing connection')
    parser.add_argument('-p', '--dest-port', required=True, default=argparse.SUPPRESS,
                        help='destination port for outgoing connection')
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
        f'Tunneling traffic from {args.listen_addr}:{args.listen_port} to {args.dest_addr}:{args.dest_port}')

    server = Server((args.listen_addr, args.listen_port),
                    (args.dest_addr, args.dest_port), parsed_key)
    try:
        asyncio.run(server.run())
    except KeyboardInterrupt:
        logging.info('Bye bye!')
