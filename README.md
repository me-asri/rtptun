# rtptun
__rtptun__ is a UDP tunnel which shapes UDP traffic into (fake) RTP traffic, helping you get VPN traffic through protocol whitelists.

## Features
 * Cross-platform (tested on Linux and Windows)
 * Low overhead (12 bytes)
 * XOR obfuscation

## Limitations
 * [The Parrot is Dead](https://people.cs.umass.edu/~amir/papers/parrot.pdf)
 * No encryption/authentication for proxied UDP traffic
 * No IPv6 support (for now)
 * Unfinished code

## Disclaimer
__Here be dragons!__

I'm no security expert and I've written this software just to learn a thing or two about networking. There are still a lot of stuff left undone and there might be a few nasty bugs I have not noticed.

TL;DR: Use this software at your own discretion.

## Requirements
 * Python 3.8 or higher 
 * PyCryptodome

## Usage
### Server
Assuming there's a VPN server (OpenVPN/WireGuard/...) on local UDP port 1194:
```
python3 rtptun -x MySuperSecretXORKey server -p 16384 -q 1194
```
__rtptun__ will listen globally on port 16384 for __rtptun__ clients to forward data to and from the VPN server running locally on port 1194.

You must choose a random string as key for XOR obfuscation. You can disable XOR altogether by dropping the `-x` flag.

__KEEP IN MIND: XOR IS NOT A REPLACEMENT FOR PROPER ENCRYPTION!__

__Encryption and authentication is entirely provided by your VPN software.__

### Client
```
python3 rtptun -x MySuperSecretXORKey client -l 1194 -s 198.51.100.1 -p 16384
```
__rtptun__ will listen locally on UDP port 1194 and forward traffic to __rtptun__ server running on host 198.51.100.1 and port 16384.

__Client XOR key must match server's key.__