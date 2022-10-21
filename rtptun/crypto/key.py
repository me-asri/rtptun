from base64 import b64decode, b64encode

from Crypto.Random import get_random_bytes


KEY_SIZE = 32


def gen_str_key() -> str:
    key = get_random_bytes(KEY_SIZE)
    return b64encode(key).decode()


def parse_str_key(key: str) -> bytes:
    try:
        decoded = b64decode(key)
    except ValueError:
        raise ValueError('Invalid key')

    dec_len = len(decoded)
    if dec_len != KEY_SIZE:
        raise ValueError('Invalid key')

    return decoded
