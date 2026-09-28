######################################################################################
#
# render_bigtext.py
#
# Emulation-only mockups: the rover HPA (".008") and SIV ("42") values, and the base SIV
# value, drawn larger than the 10x20 font - as large as they can get without touching any
# other icon or text on any rover / base screen. The colons stay exactly where the firmware
# puts them (10x20); the digits are centered on the colon's dot midline, spaced
# proportionally (narrow '.'), using the synthesized glyphs in bigtext_fonts.py.
#
# For each glyph variant it searches digit heights upward and keeps the largest that
# is clean on every screen, then renders every screen at that size.
#
# Use: python render_bigtext.py
# Output: output/bigtext_vN/<variant>/ (screens + sheet.png) and report.md
#
######################################################################################

from pathlib import Path

import bigtext_fonts
from display_port import Painter, arduino_float
from render_screens import LAYOUT, SCREENS, build_sheet, INSPECT_SCALE

HERE = Path(__file__).parent
BIG_NAMES = ("HPA big", "SIV big")
COLON_MIDLINE = 11.5  # 10x20 ':' dots on glyph rows 8-9 and 14-15
COLON_INK_END = 6  # 10x20 ':' ink columns 4-6


class BigTextPainter(Painter):
    glyphs = None
    gap = 2

    def _digits_top(self, colon_y):
        height = self.glyphs["0"].shape[0]
        return round(colon_y + COLON_MIDLINE - height / 2)

    def paint_horizontal_accuracy(self, x, y):
        hpa = self.s.hpa
        if hpa > 30.0:
            value = ">30m"
        elif hpa >= 10.0:
            value = arduino_float(hpa, 1)
        elif hpa >= 1.0:
            value = arduino_float(hpa, 2)
        else:
            value = "." + "%03d" % int(hpa * 1000)
        p = self.panel
        colon_y = y - self.L.colon_raise
        with p.element("HPA colon"):
            p.set_font("10X20")
            p.set_cursor(x, colon_y)
            p.print(":")
        with p.element("HPA big"):
            bigtext_fonts.draw_text(p, self.glyphs, x + COLON_INK_END + 3, self._digits_top(colon_y), value, self.gap)

    def paint_siv_text(self, coords):
        s = self.s
        x, y = coords
        p = self.panel
        colon_x, colon_y = x + self.L.siv_colon_dx, y - self.L.colon_raise
        with p.element("SIV colon"):
            p.set_font("10X20")
            p.set_cursor(colon_x, colon_y)
            p.print(":")
        siv = min(s.siv, 99)
        if not s.base and not s.fixed:
            siv = 0
        with p.element("SIV big"):
            end = bigtext_fonts.draw_text(p, self.glyphs, colon_x + COLON_INK_END + 3, self._digits_top(colon_y),
                                          str(siv), self.gap)
        self.siv_text_end_x = end + 1


def render_big(state, glyphs, gap):
    painter = BigTextPainter(state, LAYOUT)
    painter.glyphs, painter.gap = glyphs, gap
    return painter.display_update()


def problems(panel):
    """Collisions (overlap) and contact (touching) involving the big text."""
    found = []
    for kind, pairs in (("overlap", panel.overlaps()), ("touching", panel.touching())):
        for a, b, n in pairs:
            if a in BIG_NAMES or b in BIG_NAMES:
                found.append(f"{kind} `{a}` x `{b}` ({n} px)")
    return found


def gap_for(height):
    return max(2, round(height * 0.12))


def main():
    n = 1
    while (HERE / "output" / f"bigtext_v{n}").exists():
        n += 1
    out = HERE / "output" / f"bigtext_v{n}"
    report = ["# Bigger SIV / HPA mockups", "",
              "Digit height = rows of digit ink (10x20 today: 13). Colons unchanged.", ""]
    screens = [s for s in SCREENS]
    for variant, make in bigtext_fonts.VARIANTS.items():
        # Largest clean height per screen, then the largest clean on all of them
        per_screen = {}
        for sid, name, state in screens:
            best = None
            for height in range(13, 45):
                glyphs = make(height)
                if glyphs is None:
                    continue  # No size this small (bitmap fonts)
                if problems(render_big(state, glyphs, gap_for(height))):
                    break
                best = height
            per_screen[sid] = best
        # One size everywhere, set by the numeric rover screens; ">30m" (wide '>' and 'm')
        # shrinks to its own largest clean size instead of holding everything back. Base SIV
        # uses the rover size (base screens alone could go much larger).
        numeric = {sid: per_screen[sid] for (sid, _, st) in screens
                   if not st.base and st.hpa <= 30.0}
        height = min(numeric.values())
        limiting = [sid for sid, h in numeric.items() if h == height]

        (out / variant).mkdir(parents=True, exist_ok=True)
        rendered = []
        shrunk = []
        for sid, name, state in screens:
            h = min(height, per_screen[sid])
            if h < height:
                shrunk.append(f"{sid} {h} px")
            panel = render_big(state, make(h), gap_for(h))
            img = panel.to_image(INSPECT_SCALE)
            img.save(out / variant / f"{sid}_{name}.png")
            rendered.append((f"{sid} {name}", img))
        build_sheet(rendered).save(out / variant / "sheet.png")

        real = make(height)["0"].shape[0]
        report += [f"## {variant}: digit height {real} px (gap {gap_for(height)} px)", "",
                   f"- limited by: {', '.join(limiting)}",
                   f"- shrunk to fit (\">30m\"): {', '.join(shrunk) or 'none'}",
                   "- largest clean height per screen: " + ", ".join(f"{s} {h}" for s, h in per_screen.items()), ""]
        # What stops it growing one more pixel on the limiting screens
        for sid, name, state in screens:
            if sid in limiting and sid in numeric:
                why = problems(render_big(state, make(height + 1), gap_for(height + 1)))
                report.append(f"- {sid} at {height + 1} px: " + "; ".join(why))
        report.append("")
        print(f"{variant}: {real} px digits, limited by {', '.join(limiting)}")
    (out / "report.md").write_text("\n".join(report), encoding="utf-8")
    print(f"-> {out}")


if __name__ == "__main__":
    main()
