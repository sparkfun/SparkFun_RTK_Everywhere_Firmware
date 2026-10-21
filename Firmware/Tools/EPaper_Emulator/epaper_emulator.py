######################################################################################
#
# epaper_emulator.py
#
# Pixel-faithful model of the 184x88 e-paper (Good Display GDEM0097T61, SSD1680)
# graphics buffer as driven by the SparkFun SSD168x I2C Interface Library:
#
#   - 1 bit per pixel buffer, drawn through the library's raster ops (Copy / XOR)
#   - bitmap(): ports I2cSsd1680Rotated::drawBitmap() - pushes BOTH 0 and 1 bits
#     through the raster op, bit 0 of each column byte = top row
#   - text(): ports QwEpGrBufferDevice::drawText() - pushes only 1 bits
#   - write()/print(): ports the library's Arduino Print write() cursor advance
#     (pitch = font width + 1, wrap when x > 184 - font width)
#
# Glyphs come from fonts_ep.json (extract_fonts.py, straight from the library) and
# icons from icons_184x88.json (extract_icons.py, straight from icons.h).
#
# Every draw is attributed to the current "element" so the caller can produce a
# debug overlay and an overlap report (which lit pixels of which elements collide).
#
######################################################################################

import json
from contextlib import contextmanager
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).parent

PANEL_WIDTH_PX = 184
PANEL_HEIGHT_PX = 88
PANEL_ACTIVE_WIDTH_MM = 22.26
PANEL_ACTIVE_HEIGHT_MM = 10.65
PANEL_PIXEL_PITCH_MM = PANEL_ACTIVE_WIDTH_MM / PANEL_WIDTH_PX  # ~0.121mm, matches datasheet

BG_COLOR = (200, 200, 200)  # light gray - approximates the epaper "white" state
FG_COLOR = (0, 0, 0)  # black - epaper "black" state

with open(HERE / "icons_184x88.json", encoding="utf-8") as f:
    FIRMWARE = json.load(f)
ICONS = FIRMWARE["icons"]
PROPS = FIRMWARE["props"]

with open(HERE / "fonts_ep.json", encoding="utf-8") as f:
    FONTS = json.load(f)

ROP_COPY = "copy"
ROP_XOR = "xor"


def pixel_size_for_ppi(ppi: float) -> float:
    """Monitor pixels per panel pixel so the render is physically 22.26mm x 10.65mm."""
    return PANEL_PIXEL_PITCH_MM / 25.4 * ppi


def icon_ink_cols(name):
    """(first, last) source columns of an icon that contain any lit pixel."""
    icon = ICONS[name]
    w, h, data = icon["width"], icon["height"], icon["data"]
    cols = [x for x in range(w) if any(data[x + (y // 8) * w] & (1 << (y % 8)) for y in range(h))]
    return cols[0], cols[-1]


def glyph_ink_rows(font, text):
    """(top, bottom) rows, relative to the cursor y, that the text's lit pixels span."""
    panel = Panel(rop=ROP_COPY)
    panel.set_font(font)
    panel.text(0, 0, text)
    rows = [y for y in range(panel.height) if any(panel.buf[y * panel.width : (y + 1) * panel.width])]
    return rows[0], rows[-1]


class Element:
    def __init__(self, name):
        self.name = name
        self.lit = set()  # (x, y) of source-ON pixels this element drew
        self.box = None  # [x0, y0, x1, y1] inclusive, of everything it touched

    def touch(self, x, y, on):
        if on:
            self.lit.add((x, y))
        if self.box is None:
            self.box = [x, y, x, y]
        else:
            b = self.box
            b[0], b[1], b[2], b[3] = min(b[0], x), min(b[1], y), max(b[2], x), max(b[3], y)


class Panel:
    def __init__(self, width=PANEL_WIDTH_PX, height=PANEL_HEIGHT_PX, rop=ROP_XOR):
        self.width = width
        self.height = height
        self.buf = bytearray(width * height)
        self.rop = rop
        self.font = None
        self.cursor_x = 0
        self.cursor_y = 0
        self.elements = []
        self._element = None

    # ---- bookkeeping ------------------------------------------------------------

    @contextmanager
    def element(self, name):
        previous = self._element
        self._element = Element(name)
        try:
            yield self._element
        finally:
            if self._element.box is not None:
                self.elements.append(self._element)
            self._element = previous

    def erase(self):
        self.buf = bytearray(self.width * self.height)
        self.elements = []

    # ---- primitives -------------------------------------------------------------

    def draw_pixel(self, x, y, on):
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return
        i = y * self.width + x
        if self.rop == ROP_XOR:
            self.buf[i] ^= 1 if on else 0
        else:
            self.buf[i] = 1 if on else 0
        if self._element is not None:
            self._element.touch(x, y, on)

    def bitmap(self, x0, y0, name, width=None, height=None, scale=1):
        """I2cSsd1680Rotated::drawBitmap(). scale=2 pixel-doubles the icon (2x mode)."""
        icon = ICONS[name]
        bmp_w, bmp_h = icon["width"], icon["height"]
        dst_w = bmp_w if width is None else min(width, bmp_w)
        dst_h = bmp_h if height is None else min(height, bmp_h)
        if x0 >= self.width or y0 >= self.height:
            return
        data = icon["data"]
        for y in range(dst_h):
            for x in range(dst_w):
                on = bool(data[x + (y // 8) * bmp_w] & (1 << (y % 8)))
                for dy in range(scale):
                    for dx in range(scale):
                        self.draw_pixel(x0 + x * scale + dx, y0 + y * scale + dy, on)

    def bitmap_scaled(self, x0, y0, name, target_width):
        """Nearest-neighbor scale to an arbitrary (non-integer-ratio) target width,
        aspect ratio preserved - for one-off large art (e.g. the powered-off logo)
        where `bitmap()`'s integer pixel-doubling can't hit a specific target size."""
        icon = ICONS[name]
        bmp_w, bmp_h = icon["width"], icon["height"]
        target_height = round(bmp_h * target_width / bmp_w)
        data = icon["data"]
        for y in range(target_height):
            src_y = min(bmp_h - 1, y * bmp_h // target_height)
            for x in range(target_width):
                src_x = min(bmp_w - 1, x * bmp_w // target_width)
                on = bool(data[src_x + (src_y // 8) * bmp_w] & (1 << (src_y % 8)))
                self.draw_pixel(x0 + x, y0 + y, on)
        return target_width, target_height

    def set_font(self, name):
        self.font = FONTS[name]

    def text(self, x0, y0, s):
        """QwEpGrBufferDevice::drawText() - only ON pixels are drawn."""
        f = self.font
        if not s or x0 >= self.width or y0 >= self.height:
            return
        width, height = f["width"], f["height"]
        final_row_height = 8
        n_rows = height // 8
        if n_rows == 0:
            n_rows = 1
        elif height % 8:
            final_row_height = height % 8
            n_rows += 1
        margin = 1 if n_rows == 1 else 0  # 5x7 is special
        n_row_len = f["map_width"] // width
        row_bytes = f["map_width"] * n_rows
        data = f["data"]
        x = x0
        for ch in s:
            offset = ord(ch) - f["start"]
            if 0 <= offset < f["nchar"]:
                index = (offset // n_row_len) * row_bytes + (offset % n_row_len) * width
                for row in range(n_rows):
                    rows_here = final_row_height if row == n_rows - 1 else 8
                    for i in range(width):
                        byte = data[index + i + row * f["map_width"]]
                        for j in range(rows_here):
                            if byte & (1 << j):
                                self.draw_pixel(x + i, y0 + j + row * 8, True)
            x += width + margin

    def set_cursor(self, x, y):
        self.cursor_x, self.cursor_y = x, y

    def write(self, ch):
        """The library's Arduino Print write(): one glyph, then advance."""
        f = self.font
        if ch == "\n":
            self.cursor_x = 0
            self.cursor_y += f["height"]
        elif ch != "\r":
            self.text(self.cursor_x, self.cursor_y, ch)
            self.cursor_x = (self.cursor_x + f["width"] + 1) & 0xFF
            if self.cursor_x > self.width - f["width"]:
                self.cursor_x = 0
                self.cursor_y += f["height"]
        if self.cursor_y >= self.height:
            self.cursor_y = 0

    def print(self, s):
        for ch in str(s):
            self.write(ch)

    # ---- output -----------------------------------------------------------------

    def to_image(self, scale=6.0, bg=BG_COLOR, fg=FG_COLOR):
        """NEAREST upscale keeps hard pixel edges. Sub-1x (physical size on a low-PPI
        monitor) uses LANCZOS, which is closer to how the tiny panel reads to the eye."""
        img = Image.new("RGB", (self.width, self.height), bg)
        px = img.load()
        for y in range(self.height):
            for x in range(self.width):
                if self.buf[y * self.width + x]:
                    px[x, y] = fg
        w, h = max(1, round(self.width * scale)), max(1, round(self.height * scale))
        return img.resize((w, h), Image.NEAREST if scale >= 1 else Image.LANCZOS)

    def debug_image(self, scale=6):
        """The rendered panel with every element's bounding box and name drawn over it."""
        img = self.to_image(scale)
        draw = ImageDraw.Draw(img)
        palette = [(220, 40, 40), (30, 120, 220), (20, 150, 60), (200, 120, 0), (150, 50, 200), (0, 150, 160)]
        for n, e in enumerate(self.elements):
            color = palette[n % len(palette)]
            x0, y0, x1, y1 = e.box
            draw.rectangle([x0 * scale, y0 * scale, (x1 + 1) * scale - 1, (y1 + 1) * scale - 1], outline=color, width=2)
            draw.text((x0 * scale + 3, y0 * scale + 2), e.name, fill=color)
        return img

    def overlaps(self):
        """Pairs of elements whose lit pixels collide (in XOR these cancel into holes)."""
        found = []
        for i, a in enumerate(self.elements):
            for b in self.elements[i + 1 :]:
                common = a.lit & b.lit
                if common:
                    found.append((a.name, b.name, len(common)))
        return found

    def touching(self):
        """Pairs of elements whose lit pixels don't collide but sit directly next to
        each other (no blank pixel between them, diagonals included) - they read as
        one blob on the panel."""
        found = []
        for i, a in enumerate(self.elements):
            halo = {(x + dx, y + dy) for x, y in a.lit for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
            for b in self.elements[i + 1 :]:
                if not (a.lit & b.lit) and (halo & b.lit):
                    found.append((a.name, b.name, len(halo & b.lit)))
        return found
