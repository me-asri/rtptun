from typing import Tuple, Union
from Crypto.Cipher import ChaCha20_Poly1305

CHACHA_NONCE_LEN = 12
CHACHA_TAG_LEN = 16
CHACHA_OVERHEAD = CHACHA_NONCE_LEN + CHACHA_TAG_LEN


def encrypt(data: bytes, key: bytes) -> Tuple[bytes, bytes, bytes]:
    cipher = ChaCha20_Poly1305.new(key=key)
    cipherdata, tag = cipher.encrypt_and_digest(data)

    return cipherdata, cipher.nonce, tag


def decrypt(cipherdata: bytes, key: bytes, nonce: bytes, tag: bytes) -> Union[bytes, None]:
    cipher = ChaCha20_Poly1305.new(key=key, nonce=nonce)

    try:
        data = cipher.decrypt_and_verify(cipherdata, tag)
    except ValueError:
        return None
    return data
