from itertools import repeat, compress, groupby
import base64, logging

_LOGGER = logging.getLogger(__name__)


def ilen(iterable):
    """
    Return the number of items in iterable.
    """
    return sum(compress(repeat(1), zip(iterable)))

def rle_encode(iterable: list) -> list:
    result = []
    ll = 0
    for k, g in groupby(iterable):
        l = ilen(g)
        while l > 255:
            result.extend([255, k])
            l -= 255
        result.extend([l, k])
    return result

def pack_data(prefix: bytes, data: bytes) -> str:
    rle_data = rle_encode(data)
    use_rle = len(rle_data) < len(data)
    _LOGGER.debug(f"pack_data: {len(rle_data)} vs. {len(data)}")
    if use_rle:
        return base64.standard_b64encode(prefix + bytes([1]) + bytes(rle_data)).decode("ascii")
    else:
        return base64.standard_b64encode(prefix + bytes([0]) + data).decode("ascii")