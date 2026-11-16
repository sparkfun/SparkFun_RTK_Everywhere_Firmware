######################################################################################
#
# render_proposal.py
#
# Mockups for the 184x88 e-paper redesign (epaper_redesign_plan.md). Each scenario is
# shown three ways, side by side:
#
#   Today      the current firmware layout, through the Display.ino port (display_port.py)
#   Juan       Juan Cortes's own frames, taken from his PDF (FP-E_Display_Proposal_.pdf) and
#              reduced from 2x to the native 184x88 - assets/juan_proposal/frames.json
#   Proposed   Juan's frame with the plan's changes patched in: the original LoRa, USB, NTRIP
#              and PointPerfect icons redrawn at about 1.3x, new tilt and portable icons, the Bluetooth ID hidden
#              while a phone is connected, base screens that show link health instead of a
#              packet count, a radio port icon, the heartbeat line, and the STARTING /
#              POWERED OFF screens as the firmware draws them
#
# Use: python render_proposal.py     ->  output/proposal/sheet.png (+ one PNG per frame)
#
######################################################################################

import json
from dataclasses import replace
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import render_screens as today
from display_port import render, render_boot_info, render_message, render_powered_off_firmware
from epaper_emulator import FONTS, ICONS, ROP_COPY, ROP_XOR, Panel

HERE = Path(__file__).parent
FONT_DIR = HERE / "assets" / "fonts"
OUT_DIR = HERE / "output" / "proposal"
SCALE = 3  # Sheet scale - 184x88 becomes 552x264
W, H = 184, 88

JUAN = json.loads((HERE / "assets" / "juan_proposal" / "frames.json").read_text())

# ------------------------------------------------------------------------------------
# Frames as lists of row strings ('#' = black)
# ------------------------------------------------------------------------------------


def frame_copy(sid):
    return [list(row) for row in JUAN[sid]]


def blank():
    return [["."] * W for _ in range(H)]


def fill(fr, x0, y0, x1, y1, on):
    for y in range(max(0, y0), min(H, y1 + 1)):
        for x in range(max(0, x0), min(W, x1 + 1)):
            fr[y][x] = "#" if on else "."


def stamp(fr, art, x0, y0, xor=False):
    for y, row in enumerate(art):
        for x, c in enumerate(row):
            if c == "#" and 0 <= x0 + x < W and 0 <= y0 + y < H:
                if xor:
                    fr[y0 + y][x0 + x] = "." if fr[y0 + y][x0 + x] == "#" else "#"
                else:
                    fr[y0 + y][x0 + x] = "#"


def to_panel(fr):
    panel = Panel(rop=ROP_COPY)
    for y in range(H):
        for x in range(W):
            if fr[y][x] == "#":
                panel.draw_pixel(x, y, True)
    return panel


def crop_ink(fr, x0, y0, x1, y1):
    """Ink bounding box of a region -> (art rows, left, top)."""
    pts = [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1) if fr[y][x] == "#"]
    left, right = min(p[0] for p in pts), max(p[0] for p in pts)
    top, bottom = min(p[1] for p in pts), max(p[1] for p in pts)
    return ["".join(fr[y][left : right + 1]) for y in range(top, bottom + 1)], left, top


def find(fr, art, y0=0, y1=H - 1):
    """Exact template match - returns (x, y) of each place `art` appears with y in [y0, y1]."""
    th, tw = len(art), len(art[0])
    masks = [int(row.replace("#", "1").replace(".", "0"), 2) for row in art]
    rows = [int("".join(r).replace("#", "1").replace(".", "0"), 2) for r in fr]
    full = (1 << tw) - 1
    hits = []
    for y in range(y0, min(y1, H - th) + 1):
        for x in range(0, W - tw + 1):
            shift = W - tw - x
            if all(((rows[y + r] >> shift) & full) == masks[r] for r in range(th)):
                hits.append((x, y))
    return hits


# ------------------------------------------------------------------------------------
# Fonts (BDF) - Juan's strip is Terminus Bold 20 (ter-u20b) in a 16 row strip
# ------------------------------------------------------------------------------------


class BdfFont:
    def __init__(self, filename):
        self.glyphs = {}
        lines = (FONT_DIR / filename).read_text(encoding="latin-1").splitlines()
        i = 0
        while i < len(lines):
            if lines[i].startswith("ENCODING"):
                code = int(lines[i].split()[1])
                while not lines[i].startswith("DWIDTH"):
                    i += 1
                advance = int(lines[i].split()[1])
                while not lines[i].startswith("BBX"):
                    i += 1
                w, h, xoff, yoff = map(int, lines[i].split()[1:5])
                while lines[i] != "BITMAP":
                    i += 1
                rows = [bin(int(r, 16))[2:].zfill(len(r) * 4)[:w] for r in lines[i + 1 : i + 1 + h]]
                if 32 <= code < 127:
                    self.glyphs[chr(code)] = (advance, w, h, xoff, yoff, rows)
            i += 1

    def width(self, text):
        return sum(self.glyphs[ch][0] for ch in text)

    def draw(self, fr, x, baseline, text, xor=False):
        for ch in text:
            advance, w, h, xoff, yoff, rows = self.glyphs[ch]
            art = [r.replace("1", "#").replace("0", ".") for r in rows]
            stamp(fr, art, x + xoff, baseline - (yoff + h), xor)
            x += advance
        return x


TER20 = BdfFont("ter-u20b.bdf")
TER16 = BdfFont("ter-u16b.bdf")

# ------------------------------------------------------------------------------------
# Icons
# ------------------------------------------------------------------------------------


def art(text):
    rows = text.strip("\n").split("\n")
    w = max(len(r) for r in rows)
    return [r.ljust(w, ".") for r in rows]


def firmware_icon(name):
    icon = ICONS[name]
    w, h, data = icon["width"], icon["height"], icon["data"]
    rows = ["".join("#" if data[x + (y // 8) * w] & (1 << (y % 8)) else "." for x in range(w)) for y in range(h)]
    # Trim blank rows/columns so placement uses the ink box
    while rows and "#" not in rows[0]:
        rows.pop(0)
    while rows and "#" not in rows[-1]:
        rows.pop()
    cols = [x for x in range(w) if any(r[x] == "#" for r in rows)]
    return [r[min(cols) : max(cols) + 1] for r in rows]


OUR_TILT = art("""
...#.......####.
..###.....######
.#.#.#....######
...#......######
...#.......####.
...#.......##...
...#......##....
...#......##....
...#.....##.....
...#....##......
.......##.......
.......##.......
......##........
.....##.........
....##..........
....##..........
...##...........
""")
# The original SNIP / NTRIP icon (Corr_TCP_Icon), redrawn at about 1.3x to sit with Juan's 20 px icons
OUR_NTRIP = art("""
......#.....#......
.....###...###.....
....#####.#####....
...#############...
..#######.#######..
.#######...#######.
#######.....#######
.#######...#######.
..#######.#######..
...#############...
....#####.#####....
.....###...###.....
......#.....#......
""")
# The original LoRa tower, USB trident and PointPerfect pin (Corr_Radio_LoRa, Corr_USB_Icon,
# Corr_IP_Icon), redrawn at about 1.3x to sit with Juan's 20 px icons
OUR_LORA = art("""
.#...............#.
..#......#......#..
...#....###....#...
.......#####.......
###....#####....###
.......#####.......
...#....###....#...
..#......#......#..
.#......#.#......#.
.......#####.......
.......#.#.#.......
......#.#.#.#......
......###.###......
.....#..#.#..#.....
.....#.#...#.#.....
....#####.#####....
....#.#.....#.#....
...#.#.......#.#...
..#####.....#####..
""")
OUR_USB = art("""
........#........
.......###.......
......#####......
.....#######.....
........#...#####
..###...#...#####
.#####..#...#####
#######.#...#####
.#####..#...#....
..###...#..#.....
...#....#.#......
....#...##.......
.....#..#........
......###........
.......##........
......####.......
.....######......
.....######......
......####.......
""")
OUR_PIN = art("""
.......#####.......
......#######......
.....###...###.....
.....##.....##.....
.....##.....##.....
.....###...###.....
......#######......
.......#####.......
...###..###..###...
..#......#......#..
.#...#...#...#...#.
#...#....#....#...#
#...#.........#...#
#....#.......#....#
.#....#######....#.
..#.............#..
...###.......###...
......#######......
""")
OUR_PORTABLE = art("""
.....#####.....
.....#...#.....
###############
#.............#
#.............#
#.............#
#.....###.....#
#######.#######
#.....###.....#
#.............#
#.............#
#.............#
###############
""")
OUR_RADIO_PORT = art("""
..#...........
..#...........
..#...........
..#...........
..#...........
..#...........
##############
#............#
#.####..####.#
#............#
#.##########.#
#............#
#.##########.#
#............#
##############
""")
OUR_CHECK = art("""
....................##
...................###
..................####
.................####.
................####..
...............####...
..............####....
#............####.....
##..........####......
###........####.......
.###......####........
..###....####.........
...###..####..........
....#######...........
.....#####............
......###.............
.......#..............
""")
OUR_WARNING = art("""
...........##...........
..........####..........
..........####..........
.........######.........
.........##..##.........
........###..###........
........###..###........
.......####..####.......
.......####..####.......
......#####..#####......
......#####..#####......
.....######..######.....
.....######..######.....
....#######..#######....
....################....
...########..########...
...########..########...
..######################
.#######################
########################
""")

# Juan's icons, cut from his own frames (ink bounding boxes)
JUAN_TILT, _, _ = crop_ink(JUAN["00a"], 30, 25, 48, 44)
JUAN_PORTABLE, _, _ = crop_ink(JUAN["00a"], 116, 64, 140, 87)
JUAN_NTRIP, _, _ = crop_ink(JUAN["01"], 80, 64, 104, 87)
JUAN_LORA, _, _ = crop_ink(JUAN["09"], 80, 64, 100, 87)
JUAN_USB, _, _ = crop_ink(JUAN["10"], 82, 64, 100, 87)
JUAN_CLOUD, _, _ = crop_ink(JUAN["11"], 80, 64, 110, 87)
JUAN_UP, _, _ = crop_ink(JUAN["14"], 60, 64, 80, 87)
JUAN_WIFI = crop_ink(JUAN["14"], 14, 64, 40, 87)[0]

# (Juan's icon, our replacement) - replacements are the originals at 1x, or the plan's new icons
SWAPS = [
    (JUAN_NTRIP, OUR_NTRIP),
    (JUAN_LORA, OUR_LORA),
    (JUAN_USB, OUR_USB),
    (JUAN_CLOUD, OUR_PIN),
    (JUAN_PORTABLE, OUR_PORTABLE),
]


def swap_icons(fr):
    """Replace each of Juan's icons found in the frame with ours: same center, same bottom."""
    for juan, ours in SWAPS:
        for x, y in find(fr, juan, 18):
            fill(fr, x, y, x + len(juan[0]) - 1, y + len(juan) - 1, False)
            cx = x + len(juan[0]) // 2
            bottom = y + len(juan) - 1
            stamp(fr, ours, cx - len(ours[0]) // 2, bottom - len(ours) + 1)
    for x, y in find(fr, JUAN_TILT, 18, 50):
        fill(fr, x, y, x + len(JUAN_TILT[0]) - 1, y + len(JUAN_TILT) - 1, False)
        stamp(fr, OUR_TILT, x, y + len(JUAN_TILT) - len(OUR_TILT))


def hide_id(fr):
    fill(fr, 100, 1, 157, 13, True)


def strip_word(fr, word):
    """Replace the strip's left word (Juan: ter-u20b, white on black)."""
    fill(fr, 0, 0, 99, 15, True)
    TER20.draw(fr, 4, 14, word, xor=True)


def heartbeat(fr, fraction=0.4):
    fill(fr, 0, 17, int(W * fraction) - 1, 17, True)


# ------------------------------------------------------------------------------------
# Proposed screens
# ------------------------------------------------------------------------------------


def rover(sid, bt_connected):
    fr = frame_copy(sid)
    swap_icons(fr)
    if bt_connected:
        hide_id(fr)
    heartbeat(fr)
    return fr


def base(sid, word, status, links_art=None, count_text=None):
    """Base screen: strip = base mode, body = link health. status: SENDING | NO OUTPUT | NO RTCM"""
    fr = frame_copy(sid)
    swap_icons(fr)
    strip_word(fr, "NO OUTPUT" if status == "NO OUTPUT" else word)
    fill(fr, 0, 17, 125, 63, False)  # Body, left of the satellite
    if status == "SENDING":
        stamp(fr, OUR_CHECK, 2, 26)
        TER20.draw(fr, 30, 39, "SENDING")
        if count_text:
            TER16.draw(fr, 30, 58, count_text)
    else:
        stamp(fr, OUR_WARNING, 2, 25)
        TER20.draw(fr, 32, 39, status)
        TER16.draw(fr, 32, 58, "Add a link" if status == "NO OUTPUT" else "for 15 s")
    if links_art is not None:
        fill(fr, 0, 64, 115, 87, False)  # Link icons
        x = 4
        for icon in links_art:
            stamp(fr, icon, x, 85 - len(icon) + 1)
            x += len(icon[0]) + 5
    heartbeat(fr)
    return fr


def survey_ours():
    """Mean and goal on their own lines, time beside a shorter bar."""
    fr = frame_copy("16")
    fill(fr, 0, 17, 125, 63, False)
    TER20.draw(fr, 2, 33, "MEAN 1.23m")
    TER20.draw(fr, 2, 49, "GOAL 2.00m")
    fill(fr, 2, 53, 70, 61, True)  # Bar outline
    fill(fr, 4, 55, 68, 59, False)
    fill(fr, 5, 56, 5 + int(63 * 47 / 60) - 1, 58, True)  # 47 of 60 s
    TER16.draw(fr, 76, 62, "47/60s")
    heartbeat(fr)
    return fr


def power(word, detail):
    """paintPowerScreen184x88(): logo rows 3-26, word in ter-u20b from row 36, detail in 8x16 at row 62."""
    panel = Panel(rop=ROP_COPY)
    logo = ICONS["SparkPNT_PoweredOff_Logo"]
    panel.bitmap((W - logo["width"]) // 2, 3, "SparkPNT_PoweredOff_Logo")
    fr = [["#" if panel.buf[y * W + x] else "." for x in range(W)] for y in range(H)]
    TER20.draw(fr, (W - TER20.width(word)) // 2, 49, word)
    panel = to_panel(fr)
    panel.set_font("8X16")
    width = len(detail) * (FONTS["8X16"]["width"] + 1) - 1
    panel.set_cursor((W - width) // 2, 62)
    panel.print(detail)
    return panel


# ------------------------------------------------------------------------------------
# Scenarios
# ------------------------------------------------------------------------------------

TODAY = {sid: state for sid, name, state in today.SCREENS}


def t(sid, **kw):
    return render(replace(TODAY[sid], **kw) if kw else TODAY[sid], today.LAYOUT)[0]


def j(sid):
    return to_panel(frame_copy(sid))


UP = JUAN_UP
WIFI = JUAN_WIFI
LORA = OUR_LORA
NTRIP = OUR_NTRIP

# (id, title, today, juan, proposed, note)
ROWS = [
    ("00a", "Rover, every icon, logging", t("00a"), j("00a"), rover("00a", True),
     "Tilt: plumb arrow, no baseline. BT connected: ID hidden. NTRIP: SNIP icon"),
    ("00b", "Rover, logging raw (PPP)", t("00b"), j("00b"), rover("00b", True), ""),
    ("00d", "Rover, no SD card", t("00d"), j("00d"), rover("00d", True), "Portable: carry case, not a phone"),
    ("00e", "Rover + ESP-NOW", t("00e"), j("00e"), rover("00e", True), ""),
    ("01", "NTRIP, WiFi, BT, fixed", t("01"), j("01"), rover("01", True), ""),
    ("02", "NTRIP float", t("02"), j("02"), rover("02", True), ""),
    ("03", "Fixed, tilt, BT corrections", t("03"), j("03"), rover("03", True), ""),
    ("04", "WiFi, 3D fix, automotive", t("04"), j("04"), rover("04", False), "No phone: ID shown"),
    ("05", "BT only, autonomous, wrist", t("05"), j("05"), rover("05", True), ""),
    ("06", "PPP converging", t("06"), j("06"), rover("06", True), ""),
    ("07", "PPP converged", t("07"), j("07"), rover("07", True), ""),
    ("08", "ESP-NOW corrections, mower", t("08"), j("08"), rover("08", False), ""),
    ("09", "LoRa corrections, tractor", t("09"), j("09"), rover("09", True), "LoRa: original tower"),
    ("10", "USB corrections, bike, 10%", t("10"), j("10"), rover("10", False), "USB: original trident"),
    ("11", "PointPerfect IP, scooter", t("11"), j("11"), rover("11", False), "PointPerfect: original pin"),
    ("12", "No fix, searching", t("12"), j("12"), rover("12", False), ""),
    ("13", "Antenna short", t("13"), j("13"), rover("13", False), ""),
    ("14", "Base fixed, NTRIP server", t("14"), j("14"), base("14", "BASE", "SENDING", count_text="999 sent"),
     "Strip = mode, body = link health"),
    ("15", "Base fixed, ESP-NOW + LoRa", t("15"), j("15"), base("15", "BASE", "SENDING", count_text="87 sent"), ""),
    ("17", "Base temp, NTRIP caster", t("17"), j("17"), base("17", "BASECAST", "SENDING", count_text="456 sent"),
     "BASECAST keeps the IP"),
    ("18", "Base fixed, 3 links", t("18"), j("18"), base("18", "BASE", "SENDING", count_text="12k sent"), ""),
    ("N1", "Base running, NO transport", t("14", broadcasts=[], ntrip_server_online=False, net_rtcm_out=False, rtcm_packets=456),
     j("14"), base("14", "BASE", "NO OUTPUT", links_art=[]),
     "Today / Juan: count climbs with nothing sent"),
    ("N2", "Base, radio port only", t("14", broadcasts=[], ntrip_server_online=False, net_rtcm_out=False, rtcm_packets=212),
     j("14"), base("14", "BASE", "SENDING", links_art=[OUR_RADIO_PORT, UP], count_text="212 sent"),
     "New radio port icon"),
    ("N3", "Base, NTRIP up but RTCM stalled", t("14"), j("14"), base("14", "BASE", "NO RTCM", links_art=[WIFI, NTRIP]),
     "Juan has no stalled screen"),
    ("16", "Base survey-in", t("16"), j("16"), survey_ours(), "Accuracy goal added"),
    ("B2", "Boot", render_boot_info("Facet FP", "v2.4-Oct 9 2026")[0], j("B2"), power("STARTING", "v2.4-Oct 9 2026"),
     "Off screen -> STARTING: only the words change"),
    ("B3", "Powered off", render_powered_off_firmware("Facet FP")[0], j("B3"), power("POWERED OFF", "Facet FP  B4E706"), ""),
    ("B5", "Battery at 1%", render_message("Charge Battery")[0], j("OFF_BATT"), power("CHARGE BATTERY", "Facet FP  B4E706"),
     "Today: boot below 5% only"),
]


def as_panel(x):
    return x if isinstance(x, Panel) else to_panel(x)


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    fw, fh = W * SCALE, H * SCALE
    pad, label_h, header_h, margin = 18, 26, 40, 20
    width = margin * 2 + fw * 3 + pad * 2
    height = margin + header_h + len(ROWS) * (label_h + fh + pad)
    sheet = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("arial.ttf", 16)
        bold = ImageFont.truetype("arialbd.ttf", 20)
    except OSError:
        font = bold = ImageFont.load_default()
    for col, heading in enumerate(["Today", "Juan's proposal", "Proposed"]):
        draw.text((margin + col * (fw + pad), margin), heading, fill="black", font=bold)
    y = margin + header_h
    for sid, title, a, b, c, note in ROWS:
        draw.text((margin, y + 4), f"{sid}  {title}", fill="black", font=font)
        if note:
            draw.text((margin + 2 * (fw + pad), y + 4), note, fill=(160, 30, 30), font=font)
        y += label_h
        for col, frame in enumerate((a, b, c)):
            img = as_panel(frame).to_image(scale=SCALE)
            sheet.paste(img, (margin + col * (fw + pad), y))
            img.save(OUT_DIR / f"{sid}_{['today', 'juan', 'proposed'][col]}.png")
        y += fh + pad
    sheet.save(OUT_DIR / "sheet.png")
    print(f"Wrote {OUT_DIR / 'sheet.png'} ({len(ROWS)} rows)")


if __name__ == "__main__":
    main()
