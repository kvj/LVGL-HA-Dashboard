from homeassistant.core import HomeAssistant
from homeassistant.components import image


from PIL import Image
import io, logging, math, struct

_LOGGER = logging.getLogger(__name__)

def bytes_to_scaled(data: bytes, size: int) -> bytes:
    with io.BytesIO(data) as f, io.BytesIO() as fout:
        image = Image.open(f, formats=["JPEG", "PNG"])
        image.thumbnail((size, size))
        image.save(fout, image.format)
        return fout.getvalue()


def bytes_to_565_ints(data: bytes, content_type: str, size: int, le: bool = False):
    with io.BytesIO(data) as f:
        image = Image.open(f, formats=["JPEG", "PNG"])
        image.thumbnail((size, size))
        out_bytes = image.tobytes(encoder_name="raw")
        pixels = image.width * image.height
        bp = int(len(out_bytes) / pixels)
        def to_565(i: int) -> int:
            def byte_value(index: int) -> int:
                if index < len(out_bytes):
                    return out_bytes[index]
                return 0
            R = byte_value(bp * i) >> 3
            G = byte_value(bp * i + 1) >> 2
            B = byte_value(bp * i + 2) >> 3
            return (R << 11) | (G << 5) | B
        _LOGGER.debug(f"bytes_to_565: pixels {image.width}x{image.height} ~ {len(out_bytes)}, {content_type}, {bp}, {size}")
        result = []
        for i in range(0, pixels, 2):
            if le:
                result.append(struct.unpack("<i", struct.pack("<I", (to_565(i+1) << 16) | to_565(i)))[0])
            else:
                result.append(struct.unpack("<i", struct.pack(">I", (to_565(i) << 16) | to_565(i+1)))[0])
        _LOGGER.debug(f"bytes_to_565: int32s {image.width}x{image.height} ~ {len(result)}, {le}")
        return ((image.width, image.height), result)

def get_entity_by_entity_id(hass: HomeAssistant, entity_id: str) -> image.ImageEntity | None:
    component = hass.data.get(image.const.DATA_COMPONENT)
    if component is None:
        return None
    return component.get_entity(entity_id)

async def async_get_image_by_entity_id(hass: HomeAssistant, entity_id: str) -> image.Image | None:
    if entity := get_entity_by_entity_id(hass, entity_id):
        content_type = entity.content_type
        content = await entity.async_image()
        if content_type and content:
            return image.Image(content_type, content)
    return None
