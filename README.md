# rtptun
__rtptun__ is a UDP tunnel that reshapes UDP traffic as RTP, helping you get VPN traffic through protocol whitelists.

## Features
 * Asynchronous
 * Cross-platform (tested on Linux, Windows and Android)
 * Encryption (ChaCha20-Poly1305)

## Limitations
 * No replay protection
 * No forward secrecy

## Requirements
 * GCC (GCC 8 or higher recommended)
 * Make
 * Cygwin (for Windows builds)
 * libsodium
 * libev

## Building
### Installing required dependencies
#### Ubuntu/Debian
```
# apt install libev-dev libsodium-dev
```
#### Termux
```
$ pkg install libev libsodium
```
#### Alpine
```
# apk add libev-dev libsodium-dev
# apk add libsodium-static # (optional, needed for static builds)
```
#### Cygwin
 * gcc-core
 * make
 * libev-devel
 * libsodium-devel

### Compiling
There are several build types available:

#### Release build
Produces optimized binary.

Recommended. This is usually what you want to go with.
```
$ make -j$(nproc) DEBUG=0 STATIC=0
```

#### Static release build
Same as release build but produces a static binary.

Static builds do _not_ work under Windows yet.
```
$ make -j$(nproc) DEBUG=0 STATIC=1
```

#### Debug build
Produces unopimized binary with debug information.

Should only be used for development purposes.
```
$ make -j$(nproc) DEBUG=1 STATIC=0
```

### Installation
#### Release build
```
$ make install DEBUG=0 STATIC=0
```

#### Static release build
```
$ make install DEBUG=0 STATIC=1
```

## Usage
```
Usage: rtptun <action> <options>
Example:
 - Generate key: rtptun genkey
 - Run server:   rtptun server -k <KEY> -l 5004 -p 1194
 - Run client:   rtptun client -k <KEY> -l 1194 -d 192.0.2.1 -p 5004

Actions:
  client  : run as client
  server  : run as server
  genkey  : generate encryption key

Server options:
  -i : listen address (default: 0.0.0.0)
  -l : listen port (default: 5004)
  -d : destination address (default: 127.0.0.1)
  -p : destination port

Client options:
  -i : listen address (default: 127.0.0.1)
  -l : listen port
  -d : server address
  -p : server port (default: 5004)

Common options:
  -k : encryption key
  -h : show this message
  -v : verbose
```

## Example
### Generating a key
Both client and server __must__ use the same key. You can generate a new random key using:
```
$ rtptun genkey
```

### Server
Assuming there's a VPN server (OpenVPN/WireGuard/...) running on port `1194`:
```
$ rtptun server -k <KEY> -l 5004 -p 1194
```
__rtptun__ server will be listening on port `5004` for __rtptun__ clients to connect and tunnel their traffic to the VPN server running on port `1194`.

### Client
```
$ rtptun client -k <KEY> -l 1194 -d 192.0.2.1 -p 5004
```
__rtptun__ will listen locally on port `1194` and tunnel traffic to __rtptun__ server running on host `192.0.2.1` and port `5004`.

## Disclaimer
__Here be dragons!__

I'm no security expert, I've written this software just to learn a thing or two about networking.
I'm not responsible if Lum-chan invades your home and steals your cookies.

Use this software at your own discretion.

## External Libraries
 * [libsodium](https://doc.libsodium.org/) - [ISC](https://raw.githubusercontent.com/jedisct1/libsodium/master/LICENSE)
 * [libev](http://software.schmorp.de/pkg/libev.html) - [BSD-2-Clause](http://cvs.schmorp.de/libev/LICENSE?revision=1.11&view=markup&pathrev=MAIN)
 * [uthash](https://troydhanson.github.io/uthash/) - [BSD revised](https://troydhanson.github.io/uthash/license.html)
 * [Cygwin](https://www.cygwin.com/) (for Windows builds) - [LGPLv3](https://www.cygwin.com/COPYING)