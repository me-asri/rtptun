# rtptun
__rtptun__ is a UDP tunnel that reshapes UDP traffic as RTP, helping you get VPN traffic through protocol whitelists.

## Features
 * Asynchronous
 * Cross-platform (tested on Linux, Android and Windows)
 * Encryption (ChaCha20-Poly1305)

## Limitations
 * No IPv6 support (for now)
 * No replay protection
 * No perfect forward secrecy
 * [The Parrot is Dead](https://people.cs.umass.edu/~amir/papers/parrot.pdf)

## Requirements
 * Python 3.8 or higher 
 * PyCryptodome
 * uvloop (optional)

## Usage
### Server
```
usage: server.py [-h] [--gen-key] [-i LISTEN_ADDR] -l LISTEN_PORT [-d DEST_ADDR] -p DEST_PORT -k KEY

options:
  -h, --help            show this help message and exit
  --gen-key             generate random key
  -i LISTEN_ADDR, --listen-addr LISTEN_ADDR
                        listen address for incoming connection (default: 0.0.0.0)
  -l LISTEN_PORT, --listen-port LISTEN_PORT
                        listen port for incoming connection
  -d DEST_ADDR, --dest-addr DEST_ADDR
                        destination address for outgoing connection (default: 127.0.0.1)
  -p DEST_PORT, --dest-port DEST_PORT
                        destination port for outgoing connection
  -k KEY, --key KEY     encryption key
```

### Client
```
usage: client.py [-h] -l LOCAL_PORT -s SERVER_ADDR -p SERVER_PORT -k KEY

options:
  -h, --help            show this help message and exit
  -l LOCAL_PORT, --local-port LOCAL_PORT
                        local port for clients
  -s SERVER_ADDR, --server-addr SERVER_ADDR
                        remote rtptun server address
  -p SERVER_PORT, --server-port SERVER_PORT
                        remote rtptun server port
  -k KEY, --key KEY     encryption key
```

## Example
### Generating a key
Both client and server __must__ use the same key. You can generate a new random key using:
```
python -m rtptun.server --gen-key
```

### Server
Assuming there's a VPN server (OpenVPN/WireGuard/...) running on port `1194`:
```
python -m rtptun.server -l 49155 -p 1194 -k <KEY>
```
__rtptun__ server will be listening on port `49155` for __rtptun__ clients to connect and tunnel their traffic to the VPN server running locally on port `1194`.

### Client
```
python -m rtptun.client -l 1194 -s 198.51.100.1 -p 49155 -k <KEY>
```
__rtptun__ will listen locally on port `1194` and tunnel traffic to __rtptun__ server running on host `198.51.100.1` and port `49155`.

## Disclaimer
__Here be dragons!__

I'm no security expert, I've written this software just to learn a thing or two about networking.
I'm not responsible if Lum-chan invades your home and steals your cookies.

Use this software at your own discretion.