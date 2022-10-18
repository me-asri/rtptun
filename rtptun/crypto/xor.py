from typing import Union
from itertools import cycle, islice

from Crypto.Util import strxor


def xor(buffer: Union[bytearray, memoryview], key: str) -> None:
    k = bytes(islice(cycle(key.encode()), len(buffer)))
    strxor.strxor(buffer, k, buffer)
