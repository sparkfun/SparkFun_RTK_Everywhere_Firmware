######################################################################################
#
# export_epaper.py
#
# Writes the bitmaps for the 184x88 e-paper status screens (DisplayEPaper.ino):
#
#   ../../RTK_Everywhere/icons_epaper.h   icons: Juan Cortes's proposal art
#                                         (assets/juan_proposal/icons.json, cut from his PDF at
#                                         native size) plus the redrawn icons in render_proposal.py
#   ../../RTK_Everywhere/fontEPaper.h     fonts: Terminus Bold 16/20/24/32 and Spleen 32x64
#
# Bitmaps are in displayBitmap() order: column-major, 8-row bands, bit 0 = top row. Font glyphs
# are cropped to their ink, with the offset of the ink from the cell's top left kept so text
# lines up on a baseline. See epPrint() in DisplayEPaper.ino.
#
# Use: python export_epaper.py
#
######################################################################################

import json
from pathlib import Path

import render_proposal as proposal

HERE = Path(__file__).parent
FONT_DIR = HERE / "assets" / "fonts"
FIRMWARE = HERE.parent.parent / "RTK_Everywhere"
JUAN_ICONS = json.loads((HERE / "assets" / "juan_proposal" / "icons.json").read_text())

# (C name, art) - Juan's art unless noted
ICONS = [(name, JUAN_ICONS[name]) for name in (
    "bluetooth", "wifi3", "wifi2", "wifi1", "wifiNoInternet", "espNow", "arrowDown", "arrowUp",
    "satellite", "satellitePpp", "satelliteFault", "baseTemporary", "baseFixed",
    "battery3", "battery2", "battery1", "battery0",
    "logStandard", "logPpp", "logCustom", "logIdle", "writeDot",
    "modelStationary", "modelPedestrian", "modelAutomotive", "modelSea", "modelAirborne", "modelWrist",
    "modelBike", "modelMower", "modelScooter", "modelTractor")] + [
    # Redrawn for the proposal (render_proposal.py)
    ("tilt", proposal.OUR_TILT),
    ("modelPortable", proposal.OUR_PORTABLE),
    ("ntrip", proposal.OUR_NTRIP),
    ("lora", proposal.OUR_LORA),
    ("usb", proposal.OUR_USB),
    ("pointPerfect", proposal.OUR_PIN),
    ("radioPort", proposal.OUR_RADIO_PORT),
    ("check", proposal.OUR_CHECK),
    ("warning", proposal.OUR_WARNING),
]

PRINTABLE = "".join(chr(c) for c in range(32, 127))

# (C name, BDF file, characters, comment)
BDF_FONTS = [
    ("epFontText16", "ter-u16b.bdf", PRINTABLE, "Terminus Bold 16 (10 px capitals) - small text"),
    ("epFontText20", "ter-u20b.bdf", PRINTABLE, "Terminus Bold 20 (13 px capitals) - top strip and labels"),
    ("epFontCount", "ter-u24b.bdf", "0123456789", "Terminus Bold 24 (15 px digits) - satellite count"),
    ("epFontText24", "ter-u24b.bdf", PRINTABLE, "Terminus Bold 24 (15 px capitals) - setup menu"),
    ("epFontUnits", "ter-u32b.bdf", "cm", "Terminus Bold 32 (14 px x-height) - accuracy units"),
]


def read_bdf(filename, chars):
    """Return (ascent, {char: (advance, xoff, top, rows)}) - rows are the ink, cropped."""
    lines = (FONT_DIR / filename).read_text(encoding="latin-1").splitlines()
    ascent = next(int(l.split()[1]) for l in lines if l.startswith("FONT_ASCENT"))
    wanted = {ord(ch): ch for ch in chars}
    glyphs = {}
    i = 0
    while i < len(lines):
        if lines[i].startswith("ENCODING") and int(lines[i].split()[1]) in wanted:
            ch = wanted[int(lines[i].split()[1])]
            while not lines[i].startswith("DWIDTH"):
                i += 1
            advance = int(lines[i].split()[1])
            while not lines[i].startswith("BBX"):
                i += 1
            w, h, xoff, yoff = map(int, lines[i].split()[1:5])
            while lines[i] != "BITMAP":
                i += 1
            rows = ["".join("#" if b == "1" else "." for b in bin(int(r, 16))[2:].zfill(len(r) * 4)[:w])
                    for r in lines[i + 1 : i + 1 + h]]
            top = ascent - (yoff + h)
            glyphs[ch] = crop(advance, xoff, top, rows)
        i += 1
    missing = set(chars) - set(glyphs)
    assert not missing, f"{filename} is missing {missing}"
    return ascent, glyphs


def crop(advance, xoff, top, rows):
    """Crop rows to their ink, moving xoff/top to match. A blank glyph (space) keeps no rows."""
    inked = [r for r, row in enumerate(rows) if "#" in row]
    if not inked:
        return advance, 0, 0, []
    rows = rows[inked[0] : inked[-1] + 1]
    cols = [c for c in range(len(rows[0])) if any(row[c] == "#" for row in rows)]
    rows = [row[cols[0] : cols[-1] + 1] for row in rows]
    return advance, xoff + cols[0], top + inked[0], rows


def big_digits():
    """Spleen 32x64 digits, '>' and '.' for the accuracy number, laid out as in Juan's frames:
    every digit sits in a 24 x 40 cell (the ink box of all ten digits) on a 28 px pitch; '.' is
    4 px wide on the cell's bottom with 4 px after it."""
    ascent, glyphs = read_bdf("spleen-32x64.bdf", "0123456789>.")
    digits = [glyphs[ch] for ch in "0123456789"]
    left = min(g[1] for g in digits)
    right = max(g[1] + len(g[3][0]) for g in digits)
    top = min(g[2] for g in digits)
    bottom = max(g[2] + len(g[3]) for g in digits)
    assert (right - left, bottom - top) == (24, 40), (right - left, bottom - top)
    out = {}
    for ch, (advance, xoff, gtop, rows) in glyphs.items():
        if ch == ".":
            out[ch] = (8, 0, 40 - len(rows), rows)
        else:
            out[ch] = (28, xoff - left, gtop - top, rows)
    return 40, out


def bitmap_bytes(rows):
    if not rows:
        return []
    height, width = len(rows), len(rows[0])
    data = []
    for band in range((height + 7) // 8):
        for col in range(width):
            byte = 0
            for bit in range(8):
                row = band * 8 + bit
                if row < height and rows[row][col] == "#":
                    byte |= 1 << bit
            data.append(byte)
    return data


def c_array(name, data):
    out = [f"static const uint8_t {name}[] = {{"]
    for start in range(0, len(data), 16):
        out.append("    " + ", ".join(f"0x{b:02X}" for b in data[start : start + 16]) + ",")
    out.append("};")
    return out


def char_name(ch):
    return ch if ch.isalnum() else f"x{ord(ch):02X}"


def write_icons():
    out = ["// Generated by Firmware/Tools/EPaper_Emulator/export_epaper.py - do not edit.",
           "// 184x88 e-paper status screen icons, at native size. Juan Cortes's proposal art",
           "// (Tools/EPaper_Emulator/assets/juan_proposal/icons.json) plus the redrawn tilt, portable,",
           "// NTRIP, LoRa, USB, PointPerfect, radio port, check and warning icons (render_proposal.py).",
           "// Used by DisplayEPaper.ino only - the OLEDs keep icons.h.",
           "",
           "#ifndef __ICONS_EPAPER_H__",
           "#define __ICONS_EPAPER_H__",
           "",
           "typedef struct",
           "{",
           "    uint8_t width;",
           "    uint8_t height;",
           "    const uint8_t *bitmap;",
           "} epIcon_t;",
           ""]
    for name, art in ICONS:
        assert len({len(r) for r in art}) == 1, name
        out += c_array(f"epIconBits_{name}", bitmap_bytes(art))
        out.append(f"const epIcon_t epIcon_{name} = {{{len(art[0])}, {len(art)}, epIconBits_{name}}};")
        out.append("")
    out.append("#endif // __ICONS_EPAPER_H__")
    (FIRMWARE / "icons_epaper.h").write_text("\n".join(out) + "\n", encoding="utf-8")


def write_fonts():
    out = ["// Generated by Firmware/Tools/EPaper_Emulator/export_epaper.py - do not edit.",
           "// 184x88 e-paper status screen fonts. Terminus Font Bold 4.49.1 (SIL OFL 1.1, see",
           "// Tools/EPaper_Emulator/assets/fonts/Terminus-OFL.txt) and Spleen 2.1.0 (BSD 2-Clause,",
           "// see Spleen-LICENSE.txt). Each glyph is cropped to its ink: xOffset/top place the ink",
           "// relative to the cell's left edge and the font's top line (ascent rows above the",
           "// baseline). See epPrint() in DisplayEPaper.ino.",
           "",
           "#ifndef __FONT_EPAPER_H__",
           "#define __FONT_EPAPER_H__",
           "",
           "typedef struct",
           "{",
           "    char character;",
           "    uint8_t advance;",
           "    int8_t xOffset;",
           "    uint8_t top;",
           "    uint8_t width;",
           "    uint8_t height;",
           "    const uint8_t *bitmap;",
           "} epGlyph_t;",
           "",
           "typedef struct",
           "{",
           "    uint8_t ascent;",
           "    uint8_t glyphCount;",
           "    const epGlyph_t *glyphs;",
           "} epFont_t;",
           ""]
    fonts = [(name, *read_bdf(filename, chars), comment) for name, filename, chars, comment in BDF_FONTS]
    # Terminus 20 on a 9 px advance, as in Juan's frames: 1 px between the letters, not the font's 2
    text20 = fonts[1][2]
    for ch, (advance, xoff, top, rows) in text20.items():
        text20[ch] = (advance - 1, xoff, top, rows)
    # Terminus 24 text on an 11 px advance, matching the tighter Terminus 20
    text24 = fonts[3][2]
    for ch, (advance, xoff, top, rows) in text24.items():
        text24[ch] = (advance - 1, xoff, top, rows)
    # Units: 2 px between letters ("mm" in Juan's frames), not the font's 3
    units = fonts[4][2]
    for ch, (advance, xoff, top, rows) in units.items():
        units[ch] = (len(rows[0]) + 2, xoff, top, rows)
    fonts.append(("epFontBig", *big_digits(), "Spleen 32x64 (40 px digits) - accuracy number, 28 px pitch"))
    for name, ascent, glyphs, comment in fonts:
        entries = []
        for ch, (advance, xoff, top, rows) in glyphs.items():
            bits = "nullptr"
            if rows:
                out += c_array(f"{name}_{char_name(ch)}", bitmap_bytes(rows))
                bits = f"{name}_{char_name(ch)}"
            literal = {"'": "'\\''", "\\": "'\\\\'"}.get(ch, f"'{ch}'")
            width, height = (len(rows[0]), len(rows)) if rows else (0, 0)
            entries.append(f"    {{{literal}, {advance}, {xoff}, {top}, {width}, {height}, {bits}}},")
        out.append(f"static const epGlyph_t {name}Glyphs[] = {{")
        out += entries
        out.append("};")
        out.append(f"const epFont_t {name} = {{{ascent}, {len(glyphs)}, {name}Glyphs}}; // {comment}")
        out.append("")
    out.append("#endif // __FONT_EPAPER_H__")
    (FIRMWARE / "fontEPaper.h").write_text("\n".join(out) + "\n", encoding="utf-8")
    for name, ascent, glyphs, comment in fonts:
        sample = {ch: glyphs[ch][:3] for ch in list(glyphs)[:3]}
        print(f"{name}: ascent {ascent}, {len(glyphs)} glyphs, e.g. (advance, xoff, top) {sample}")


def main():
    write_icons()
    write_fonts()
    print(f"Wrote {FIRMWARE / 'icons_epaper.h'} and {FIRMWARE / 'fontEPaper.h'}")


if __name__ == "__main__":
    main()
