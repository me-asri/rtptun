import argparse

from client import RTPTunClient
from server import RTPTunServer

parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(required=True,
                                   help='operation mode', dest='mode')

parser_client = subparsers.add_parser('client',
                                      help='run in client mode')
parser_client.add_argument('-l', '--local-port', required=True,
                           help='Local port for incoming connection')
parser_client.add_argument('-s', '--server-addr', required=True,
                           help='Server address')
parser_client.add_argument('-p', '--server-port', required=True,
                           help='Server port')

parser_server = subparsers.add_parser('server',
                                      help='run in server mode')
parser_server.add_argument('-s', '--source-addr', required=True,
                           help='Source address for incoming connection')
parser_server.add_argument('-p', '--source-port', required=True,
                           help='Source port for incoming connection')
parser_server.add_argument('-d', '--dest-addr', required=True,
                           help='Destination address for outgoing connection')
parser_server.add_argument('-q', '--dest-port', required=True,
                           help='Destination port for outgoing connection')

args = vars(parser.parse_args())

if args['mode'] == 'client':
    client = RTPTunClient(int(args['local_port']),
                          args['server_addr'], int(args['server_port']))
    client.run()
elif args['mode'] == 'server':
    server = RTPTunServer(args['source_addr'], int(args['source_port']),
                          args['dest_addr'], int(args['dest_port']))
    server.run()
    pass
