from typing import Union

MIN_KEY_LEN = 16


def xor(buffer: Union[bytearray, memoryview], key: str) -> None:
    key_bytes = key.encode()

    buffer_len = len(buffer)
    key_len = len(key)

    if key_len < MIN_KEY_LEN:
        raise ValueError(
            f'Key length cannot be less than {MIN_KEY_LEN} characters')

    idx = 0
    while idx < buffer_len:
        buffer[idx] = buffer[idx] ^ key_bytes[idx % key_len]
        idx += 1
