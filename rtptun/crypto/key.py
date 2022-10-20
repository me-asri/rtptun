from base64 import b64decode, b64encode

from Crypto.Random import get_random_bytes


def gen_str_key(key_size: int = 256) -> str:
    if key_size != 128 and key_size != 256:
        raise ValueError('Invalid key size')

    nbytes = key_size // 8

    key = get_random_bytes(nbytes)
    return b64encode(key).decode()


def parse_str_key(key: str) -> bytes:
    try:
        decoded = b64decode(key)
    except ValueError:
        raise ValueError('Invalid key')

    dec_len = len(decoded)
    if dec_len != 32 and dec_len != 256:
        raise ValueError('Invalid key')

    return decoded
