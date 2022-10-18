# rtptun
__rtptun__ is a UDP tunnel which shapes UDP traffic into (fake) RTP traffic, helping you get VPN traffic through protocol whitelists.

## Features
 * Cross-platform (tested on Linux and Windows)
 * Low overhead (12 bytes)
 * XOR obfuscation

## Limitations
 * No IPv6 support (for now)
 * No encryption or authentication
 * [The Parrot is Dead](https://people.cs.umass.edu/~amir/papers/parrot.pdf)

## Requirements
 * Python 3.8 or higher 
 * PyCryptodome

## Usage
### Server
```
usage: server.py [-h] [-x KEY] [-s SOURCE_ADDR] -p SOURCE_PORT [-d DEST_ADDR] -q DEST_PORT

options:
  -h, --help            show this help message and exit
  -x KEY, --xor KEY     XOR payload bytes with key (NOT A REPLACEMENT FOR PROPER ENCRYPTION!)
  -s SOURCE_ADDR, --source-addr SOURCE_ADDR
                        Source address for incoming connection [0.0.0.0]
  -p SOURCE_PORT, --source-port SOURCE_PORT
                        Source port for incoming connection
  -d DEST_ADDR, --dest-addr DEST_ADDR
                        Destination address for outgoing connection [127.0.0.1]
  -q DEST_PORT, --dest-port DEST_PORT
                        Destination port for outgoing connection
```

### Client
```
usage: client.py [-h] [-x KEY] -l LOCAL_PORT -s SERVER_ADDR -p SERVER_PORT

options:
  -h, --help            show this help message and exit
  -x KEY, --xor KEY     XOR payload bytes with key (NOT A REPLACEMENT FOR PROPER ENCRYPTION!)
  -l LOCAL_PORT, --local-port LOCAL_PORT
                        Local port for incoming connection
  -s SERVER_ADDR, --server-addr SERVER_ADDR
                        Server address
  -p SERVER_PORT, --server-port SERVER_PORT
                        Server port
```

## Example
### Server
Assuming there's a VPN server (OpenVPN/WireGuard/...) on local UDP port 1194:
```
python -m rtptun.server -x MySuperSecretXORKey -p 16384 -q 1194
```
__rtptun__ will listen globally on port 16384 for __rtptun__ clients to forward data to and from the VPN server running locally on port 1194.

You must choose a random string as key for XOR obfuscation. You can disable XOR altogether by dropping the `-x` flag.

__KEEP IN MIND: XOR IS NOT A REPLACEMENT FOR PROPER ENCRYPTION!__

__Encryption and authentication is entirely provided by your VPN software.__

### Client
```
python -m rtptun.client -x MySuperSecretXORKey -l 1194 -s 198.51.100.1 -p 16384
```
__rtptun__ will listen locally on UDP port 1194 and forward traffic to __rtptun__ server running on host 198.51.100.1 and port 16384.

__Client XOR key must match server's key.__

## Disclaimer
__Here be dragons!__

I'm no security expert and I've written this software just to learn a thing or two about networking.
Use this software at your own discretion.