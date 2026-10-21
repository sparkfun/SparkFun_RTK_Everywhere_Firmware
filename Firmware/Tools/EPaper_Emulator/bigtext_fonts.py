######################################################################################
#
# bigtext_fonts.py
#
# Synthesized larger digit glyphs for the "bigger SIV / HPA" mockups (render_bigtext.py).
# Emulation only - nothing here exists in the firmware or the SSD168x library yet. Each
# glyph is a 2D bool array (rows x cols) cropped to its ink, so text can be spaced
# proportionally (a narrow '.').
#
#   scaled_10x20(height) - the 10x20 font the display already uses, resampled (Lanczos +
#                          threshold) so digit ink is `height` rows tall
#   condensed(height)    - the same, squeezed to 75% width, so it can grow taller in the
#                          same horizontal space
#
######################################################################################

from functools import lru_cache
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from epaper_emulator import ROP_COPY, Panel

DIGIT_TOP_10X20, DIGIT_BOTTOM_10X20 = 3, 15  # 10x20 digit / capital ink rows


def _source_glyph(font, ch, top, bottom):
    """One character from a library font as a bool array, rows top..bottom, full cell width."""
    panel = Panel(rop=ROP_COPY)
    panel.set_font(font)
    panel.text(0, 0, ch)
    w = panel.font["width"]
    rows = []
    for y in range(top, bottom + 1):
        rows.append([bool(panel.buf[y * panel.width + x]) for x in range(w)])
    return np.array(rows, dtype=bool)


def _resample(glyph, scale, x_ratio=1.0):
    """Scale a bool glyph by `scale` (width also by x_ratio): Lanczos on 0/255, threshold 50%."""
    h, w = glyph.shape
    img = Image.fromarray((glyph * 255).astype(np.uint8))
    size = (max(1, round(w * scale * x_ratio)), max(1, round(h * scale)))
    out = np.array(img.resize(size, Image.LANCZOS)) >= 128
    return out


def _crop_columns(glyph):
    cols = np.where(glyph.any(axis=0))[0]
    if len(cols) == 0:
        return glyph[:, :0]
    return glyph[:, cols[0] : cols[-1] + 1]


def scaled_10x20(height):
    """dict char -> bool glyph, digit ink `height` rows tall. Glyphs keep their position
    relative to the digit ink box (so '.' sits on the baseline, 'm' is x-height)."""
    scale = height / (DIGIT_BOTTOM_10X20 - DIGIT_TOP_10X20 + 1)
    glyphs = {}
    for ch in "0123456789.>mN/A":
        src = _source_glyph("10X20", ch, DIGIT_TOP_10X20, DIGIT_BOTTOM_10X20)
        glyphs[ch] = _crop_columns(_resample(src, scale))
    return glyphs


def condensed(height):
    """scaled_10x20() squeezed horizontally to 75% - taller digits in the same width."""
    scale = height / (DIGIT_BOTTOM_10X20 - DIGIT_TOP_10X20 + 1)
    glyphs = {}
    for ch in "0123456789.>mN/A":
        src = _source_glyph("10X20", ch, DIGIT_TOP_10X20, DIGIT_BOTTOM_10X20)
        glyphs[ch] = _crop_columns(_resample(src, scale, 0.75))
    return glyphs


FONT_DIR = Path(__file__).parent / "assets" / "fonts"
CHARS = "0123456789.>mN/A"


def _crop_to_digit_box(canvas, chars):
    """canvas: char -> 2D bool array, all drawn at the same origin/baseline. Crop every glyph to
    the rows the digits span (top of digit ink to the baseline) and to its own ink columns."""
    rows = np.zeros(next(iter(canvas.values())).shape[0], dtype=bool)
    for ch in "0123456789":
        rows |= canvas[ch].any(axis=1)
    top, bottom = np.where(rows)[0][[0, -1]]
    return {ch: _crop_columns(canvas[ch][top : bottom + 1]) for ch in chars}


@lru_cache(maxsize=None)
def _ttf_at(path, size, variation):
    """Hinted 1-bit FreeType rendering (no anti-aliasing) of CHARS at pixel size `size`."""
    font = ImageFont.truetype(str(path), size)
    if variation:
        font.set_variation_by_name(variation)
    canvas = {}
    for ch in CHARS:
        img = Image.new("1", (size * 2, size * 2), 0)
        draw = ImageDraw.Draw(img)
        draw.fontmode = "1"
        draw.text((size // 2, size // 4), ch, font=font, fill=1)
        canvas[ch] = np.array(img, dtype=bool)
    return _crop_to_digit_box(canvas, CHARS)


def ttf(filename, variation=None):
    """Generator: the largest pixel size whose digits are at most `height` rows tall."""
    path = FONT_DIR / filename

    def make(height):
        best = None
        for size in range(10, 80):
            glyphs = _ttf_at(path, size, variation)
            if glyphs["0"].shape[0] > height:
                break
            best = glyphs
        return best

    return make


@lru_cache(maxsize=None)
def _bdf(filename):
    """Minimal BDF reader: char -> 2D bool array on a common baseline canvas."""
    lines = (FONT_DIR / filename).read_text(encoding="latin-1").splitlines()
    ascent = next(int(l.split()[1]) for l in lines if l.startswith("FONT_ASCENT"))
    descent = next(int(l.split()[1]) for l in lines if l.startswith("FONT_DESCENT"))
    wanted = {ord(ch): ch for ch in CHARS}
    canvas, i = {}, 0
    while i < len(lines):
        if lines[i].startswith("ENCODING") and int(lines[i].split()[1]) in wanted:
            ch = wanted[int(lines[i].split()[1])]
            while not lines[i].startswith("BBX"):
                i += 1
            w, h, xoff, yoff = map(int, lines[i].split()[1:5])
            while lines[i] != "BITMAP":
                i += 1
            rows = lines[i + 1 : i + 1 + h]
            grid = np.zeros((ascent + descent, w + max(0, xoff) + 2), dtype=bool)
            top = ascent - (yoff + h)
            for r, hexrow in enumerate(rows):
                bits = bin(int(hexrow, 16))[2:].zfill(len(hexrow) * 4)
                for c in range(w):
                    if bits[c] == "1":
                        grid[top + r, max(0, xoff) + c] = True
            canvas[ch] = grid
        i += 1
    width = max(g.shape[1] for g in canvas.values())
    canvas = {ch: np.pad(g, ((0, 0), (0, width - g.shape[1]))) for ch, g in canvas.items()}
    return _crop_to_digit_box(canvas, CHARS)


def bdf(*filenames):
    """Generator: the largest of these bitmap font sizes whose digits are at most `height` tall."""

    def make(height):
        best = None
        for filename in filenames:
            glyphs = _bdf(filename)
            if glyphs["0"].shape[0] <= height and (best is None or glyphs["0"].shape[0] > best["0"].shape[0]):
                best = glyphs
        return best

    return make


VARIANTS = {
    "scaled_10x20": scaled_10x20,
    "dejavu_sans_mono_bold": ttf("DejaVuSansMono-Bold.ttf"),
    "dejavu_condensed_bold": ttf("DejaVuSansCondensed-Bold.ttf"),
    "cascadia_mono_bold": ttf("CascadiaMono.ttf", "Bold"),
    "ibm_plex_mono_bold": ttf("IBMPlexMono-Bold.ttf"),
    "terminus_bold": bdf("ter-u20b.bdf", "ter-u22b.bdf", "ter-u24b.bdf", "ter-u28b.bdf", "ter-u32b.bdf"),
    "spleen": bdf("spleen-12x24.bdf", "spleen-16x32.bdf"),
}


# The set the firmware ships (export_bigdigits.py -> fontBigDigits.h) and its fit rule:
# (digit height, gap) pairs tried in order by printBigDigits() in Display.ino, and by
# display_port._print_big_digits() in the emulator. Keep all three in step.
FIRMWARE_VARIANT = "terminus_bold"
FIRMWARE_FIT = ((20, 3), (20, 2), (20, 1), (18, 2), (18, 1))


def firmware_glyphs(height):
    """FIRMWARE_VARIANT glyphs at exactly `height` digit rows."""
    glyphs = VARIANTS[FIRMWARE_VARIANT](height)
    if glyphs is None or glyphs["0"].shape[0] != height:
        raise ValueError(f"{FIRMWARE_VARIANT} has no {height} px digit size")
    return glyphs


def text_width(glyphs, text, gap):
    return sum(glyphs[ch].shape[1] for ch in text) + gap * (len(text) - 1)


def draw_text(panel, glyphs, x, y_top, text, gap):
    """Draw with each glyph's ink starting at x; y_top is the digit ink box's top row."""
    for ch in text:
        g = glyphs[ch]
        for r, c in zip(*np.nonzero(g)):
            panel.draw_pixel(x + int(c), y_top + int(r), True)
        x += g.shape[1] + gap
    return x - gap  # one past the last inked column
