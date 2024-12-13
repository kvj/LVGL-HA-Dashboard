import json, pathlib, base64, logging
from PIL import ImageFont
from dataclasses import dataclass
from .icon import DEFAULT_ICON
from .compress import rle_encode

_LOGGER = logging.getLogger(__name__)


def locate_dir():
    return __path__[0]

@dataclass
class GlyphInfo:
    size: int
    x: int
    y: int
    width: int
    height: int
    data: list

    def __str__(self) -> str:
        return f"GlyphInfo[{self.size}]: offset = {self.x}x{self.y}, size = {self.width}x{self.height}, data = {len(self.data)}"

class GlyphProvider:

    def __init__(self) -> None:
        self._font_cache = {}

    def init(self):
        self._glyph_map = self._load_meta_json()

    def _load_meta_json(self) -> dict:
        path = pathlib.Path(locate_dir()).joinpath("meta.json")
        data = json.loads(path.read_text())
        result = {}
        aliases = {}
        for item in data:
            codepoint = chr(int(item["codepoint"], 16))
            result[item["name"]] = codepoint
            for a in item.get("aliases", []):
                aliases[a] = codepoint
        for a, codepoint in aliases.items():
            if a not in result:
                result[a] = codepoint
        return result
    
    def _load_ttf_font(self, size: int):
        if size not in self._font_cache:
            path = pathlib.Path(locate_dir()).joinpath("mdi-webfont.ttf")
            ttf_font = ImageFont.truetype(path, size)
            self._font_cache[size] = ttf_font
        return self._font_cache[size]
    
    def _get_mask(self, font, glyph):
        return font.getmask(glyph, mode="1")
    
    def _get_offset(self, font, glyph):
        _, (offset_x, offset_y) = font.font.getsize(glyph)
        return offset_x, offset_y
    
    def get_icon_value(self, icon: str, size: int) -> dict:
        if icon not in self._glyph_map:
            icon = DEFAULT_ICON
        info = self.get_glyph(icon, size)
        rle_data = rle_encode(info.data)
        use_rle = len(rle_data) < len(info.data)
        # if use_rle:
        _LOGGER.debug(f"get_icon_value: {icon}, {use_rle}, {len(info.data)} / {len(rle_data)}")
        data = [info.x, info.y, info.width, info.height, 1 if use_rle else 0] + (rle_data if use_rle else info.data)
        return {"name": icon, "size": size, "data": base64.standard_b64encode(bytearray(data)).decode("ascii")}

    def get_glyph(self, icon: str, size: int) -> GlyphInfo | None:
        glyph = self._glyph_map.get(icon)
        ttf_font = self._load_ttf_font(size)

        mask = self._get_mask(ttf_font, glyph)
        offset_x, offset_y = self._get_offset(ttf_font, glyph)

        width, height = mask.size

        bpp = 1
        scale = 1

        glyph_data = [0] * ((height * width * bpp + 7) // 8)
        pos = 0
        for y in range(height):
            for x in range(width):
                pixel = mask.getpixel((x, y)) // scale
                for bit_num in range(bpp):
                    if pixel & (1 << (bpp - bit_num - 1)):
                        glyph_data[pos // 8] |= 0x80 >> (pos % 8)
                    pos += 1
        return GlyphInfo(size, offset_x, offset_y, width, height, glyph_data)
