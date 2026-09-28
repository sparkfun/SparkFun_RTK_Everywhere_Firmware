######################################################################################
#
# compare_bigtext.py
#
# One sheet comparing every big digit candidate from a render_bigtext.py run: a close-up
# of the glyph set (as the panel would show it, 1 px = 1 dot) and three screens per font.
#
# Use: python compare_bigtext.py output/bigtext_vN
#
######################################################################################

import glob
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import bigtext_fonts

SCREENS = ("00a", "02", "14")
BG, INK = (200, 200, 200), (0, 0, 0)


def glyph_strip(glyphs, text="0123456789.>m", gap=3, scale=6):
    height = glyphs["0"].shape[0]
    width = bigtext_fonts.text_width(glyphs, text, gap)
    img = Image.new("RGB", (width + 8, height + 8), BG)
    px = img.load()
    x = 4
    for ch in text:
        for r, c in zip(*np.nonzero(glyphs[ch])):
            px[x + int(c), 4 + int(r)] = INK
        x += glyphs[ch].shape[1] + gap
    return img.resize((img.width * scale, img.height * scale), Image.NEAREST)


def main(run_dir):
    run = Path(run_dir)
    report = (run / "report.md").read_text(encoding="utf-8")
    rows = []
    for variant, make in bigtext_fonts.VARIANTS.items():
        line = next(l for l in report.splitlines() if l.startswith(f"## {variant}:"))
        px = int(line.split("digit height ")[1].split(" px")[0])
        glyphs = next(make(h) for h in range(px, 60) if make(h) is not None and make(h)["0"].shape[0] == px)
        cells = [glyph_strip(glyphs)]
        for sid in SCREENS:
            path = sorted(glob.glob(str(run / variant / f"{sid}_*.png")))[0]
            img = Image.open(path)
            cells.append(img.resize((img.width // 2, img.height // 2), Image.NEAREST))
        rows.append((f"{variant} - {px} px digits", cells))

    col_w = [max(r[1][i].width for r in rows) for i in range(len(rows[0][1]))]
    row_h = [max(c.height for c in r[1]) + 24 for r in rows]
    sheet = Image.new("RGB", (sum(col_w) + 20 * len(col_w) + 20, sum(row_h) + 20 * len(rows) + 20), "white")
    draw = ImageDraw.Draw(sheet)
    y = 20
    for (label, cells), h in zip(rows, row_h):
        draw.text((20, y), label, fill=(0, 0, 0))
        x = 20
        for cell, w in zip(cells, col_w):
            sheet.paste(cell, (x, y + 20))
            x += w + 20
        y += h + 20
    sheet.save(run / "compare.png")
    print(f"-> {run / 'compare.png'}")


if __name__ == "__main__":
    main(sys.argv[1])
