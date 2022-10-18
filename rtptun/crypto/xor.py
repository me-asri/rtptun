from typing import Union
from itertools import cycle, islice

from Crypto.Util import strxor


def xor(buffer: bytes, key: str, output: Union[bytearray, memoryview]) -> None:
    k = bytes(islice(cycle(key.encode()), len(buffer)))
    strxor.strxor(buffer, k, output)
