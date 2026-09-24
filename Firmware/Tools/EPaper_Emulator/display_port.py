######################################################################################
#
# display_port.py
#
# Python port of the DISPLAY_184x88 paint path in RTK_Everywhere/Display.ino, driven
# by a mock State instead of firmware globals. Function names and call order follow
# the firmware so the two can be compared side by side:
#
#   displayUpdate() -> per-state paint calls; text is drawn immediately, icons are
#   queued in an icon list and drawn last, all through the sticky XOR raster op.
#
# Positions come from a Layout:
#   Layout1x - the firmware as it is today (icons.h coordinates + Display.ino offsets)
#   Layout2x - the v2 candidate 2x-icons layout (rover only)
#   Layout2xV3 - v3: smaller serial / IP fonts, raised HPA/SIV colons, 2x base screens
#   Layout2xV4 - v4: fixed top-row icon slots, large serial that drops the model digits
#                while Bluetooth is connected, SIV colon nudged left
#                (epaper_update.md Section 6.3)
#
# Full-screen messages (displayMessage(): "ESP-NOW Pairing", "GNSS / Update / 34%",
# "Shutting Down...", ...) are ported too; they contain no icons, so they look the
# same in every layout.
#
# Rover and base operational states and displayMessage() are ported. The one
# intentional deviation from today's firmware is
# State.ip_fit = "fixed": the IP address budget uses the real 10x20 pitch and stops
# short of the bottom-right icons, so long addresses ping-pong (Section 7, item 2).
#
######################################################################################

from dataclasses import dataclass, field
from typing import Optional

from epaper_emulator import FIRMWARE, FONTS, ICONS, PROPS, Panel, ROP_XOR, glyph_ink_rows, icon_ink_cols

SCALARS = FIRMWARE["scalars"]
CORRECTION = FIRMWARE["correction"]  # CORRECTION_ID_T order
BROADCAST = FIRMWARE["broadcast"]  # BCAST_ID_T order

CORRECTION_IDS = ["RADIO_EXT", "ESPNOW", "RADIO_LORA", "BLUETOOTH", "USB", "TCP", "PPP_HAS_B2B", "LBAND", "IP"]
BCAST_IDS = ["ESPNOW", "RADIO_LORA", "NTRIP_SERVER", "NTRIP_CASTER"]
BCAST_PRIORITY = ["ESPNOW", "RADIO_LORA", "NTRIP_SERVER", "NTRIP_CASTER"]  # paintBaseBroadcastIcons()

ROVER_STATES = {"ROVER_NO_FIX", "ROVER_FIX", "ROVER_RTK_FLOAT", "ROVER_RTK_FIX"}
BASE_STATES = {"BASE_TEMP_SETTLE", "BASE_TEMP_SURVEY_STARTED", "BASE_TEMP_TRANSMITTING", "BASE_FIXED_TRANSMITTING"}

DUTY_SOLID = 0b11111111
DUTY_BLINK = 0b01010101

DISPLAY_HEIGHT = 88
ICON_GAP = 2  # paintBaseBroadcastIcons() / setRadioIcons() iconGap

AX, AY = SCALARS["AccuracyIconXPos184x88"], SCALARS["AccuracyIconYPos184x88"]
SX, SY = SCALARS["SIVIconXPos184x88"], SCALARS["SIVIconYPos184x88"]

# paintBaseStats[] DISPLAY_184x88 rows (Display.ino:2842-2861):
#   name: (label x, label y, label EP font, data x, data y, data EP font)
BASE_STATS = {
    "Mean:": (AX, AY - 4, "8X16", AX + 42, AY - 8, "10X20"),
    "Time:": (SX - 2, SY - 4, "8X16", SX + 42, SY - 8, "10X20"),
    "BaseCast": (AX, SY - 9, "10X20", AX + 80, AY - 8, "10X20"),
    "Casting": (SX - 11 - 77, SY - 9, "10X20", AX + 80, AY - 8, "10X20"),
    "Xmitting": (AX, SY - 9, "10X20", AX + 80, AY - 8, "10X20"),
    "RTCM:": (SX, SY - 9, "10X20", SX + 50, SY - 9, "10X20"),
}


@dataclass
class State:
    system_state: str
    serial: str = "B4E706"
    # Radios
    bt_connected: bool = False
    wifi_station: bool = False
    wifi_internet: bool = False
    wifi_softap: bool = False
    wifi_rssi: int = -30
    espnow_paired: bool = False
    espnow_rssi: int = -30
    # RTCM activity flags (drive the up/down arrows)
    bt_rtcm_in: bool = False
    bt_rtcm_out: bool = False
    espnow_rtcm_in: bool = False
    espnow_rtcm_out: bool = False
    usb_rtcm_in: bool = False
    net_rtcm_in: bool = False
    mqtt_rx: bool = False
    net_rtcm_out: bool = False
    # GNSS
    hpa: float = 0.014
    siv: int = 24
    is_fixed: Optional[bool] = None  # None: derived from system_state
    dynamic_model: Optional[str] = "DynamicModel_1_Properties"  # default DYN_MODEL_PORTABLE
    ppp_capable: bool = False
    ppp_mode: bool = False
    ppp_converging: bool = False
    ppp_converged: bool = False
    lband_rx: bool = False
    supports_short_open: bool = False
    antenna_shorted: bool = False
    antenna_open: bool = False
    tilt_correcting: bool = False
    battery_percent: int = 90
    # Network / corrections
    ip: Optional[str] = None
    correction_source: Optional[str] = None  # one of CORRECTION_IDS
    broadcasts: list = field(default_factory=list)  # subset of BCAST_IDS
    ntrip_server_online: bool = False
    base_caster_override: bool = False
    rtcm_packets: int = 999  # Base.ino wraps it to 1 above 999 (above 99 with enableResetDisplay)
    survey_mean: float = 1.23
    survey_time: int = 47
    # Logging slot
    logging: Optional[str] = None  # None (pulse) | "STANDARD" | "PPP" | "CUSTOM"
    anim_frame: Optional[int] = None  # None: 3 when logging, 0 for pulse
    # Rendering controls
    blink_on: bool = True  # which half of a 0b01010101 blink to show
    shuttle_step: int = 0  # printFittedText() ping-pong position
    ip_fit: str = "fixed"  # "firmware" (today's bug) | "fixed" (ping-pong)
    serial_style: str = "auto"  # v4: "auto" (large, model digits dropped while BT is connected) | "small"
    correction_x_nudge: int = 0  # per-screen fine adjustment of the bottom-right correction icon
    edge_frame: bool = False  # DisplayTest scenario 19: drawFrame() on the outermost pixels
    compact_top_row: bool = False  # v4: pack bt/wifi/espnow/arrow right after the serial number
    # instead of their normal fixed slots - for the rare case where wifi AND espnow
    # are both active at once, which doesn't fit the normal fixed spacing (Section 9)

    @property
    def rover(self):
        return self.system_state in ROVER_STATES

    @property
    def base(self):
        return self.system_state in BASE_STATES

    @property
    def fixed(self):
        if self.is_fixed is not None:
            return self.is_fixed
        return self.system_state != "ROVER_NO_FIX"

    @property
    def network_has_internet(self):
        return self.wifi_station and self.wifi_internet  # FP: no Ethernet, no cellular


def arduino_float(value, digits):
    """Arduino Print::printFloat()."""
    out = ""
    if value < 0:
        out, value = "-", -value
    rounding = 0.5
    for _ in range(digits):
        rounding /= 10.0
    value += rounding
    int_part = int(value)
    remainder = value - int_part
    out += str(int_part)
    if digits > 0:
        out += "."
    for _ in range(digits):
        remainder *= 10.0
        digit = int(remainder)
        out += str(digit)
        remainder -= digit
    return out


# --------------------------------------------------------------------------------
# Layouts
# --------------------------------------------------------------------------------


class Layout1x:
    """Today's firmware: icons.h positions, Display.ino text offsets."""

    name = "1x"
    scale = 1
    serial_pos = (0, 0)  # paintSerial6digit(0, 0)
    serial_font = "10X20"
    ip_y = 68  # displayFullIPAddress()
    ip_font = "10X20"
    logging_x = PROPS["LoggingIconProperties"][0]["x"]  # DisplayWidth - Logging_Width
    colon_raise = 0  # HPA / SIV colons print inline with the value
    digit_drop = 0  # extra rows the HPA / SIV digits sit below the text y
    siv_colon_dx = 0  # SIV colon printed at the text x
    icon_swap = {}  # accuracy icon substitutions
    rtcm_x_adjust = 5  # paintRTCM(): nudge counts < 100 right

    def hpa_text(self, icon):  # displayHorizontalAccuracy() / displayRTKAccuracy()
        return icon["x"] + 16, icon["y"] - 2

    def siv_text(self, icon, base=False):  # paintSIVIcon() then paintSIVText() "textCoords.y -= 2"
        return icon["x"] + icon["width"] + 2, icon["y"] + 1 - 2

    def base_stat(self, name):
        return BASE_STATS[name]

    def base_cast_font(self):
        return "10X20"

    def base_cast_pos(self, text_width, siv_end_x):
        """(x, y) for the merged 'Cast:123' / 'Xmit:123' / 'BaseCast:123' line used
        by paint_rtcm() from v4 onward (Layout1x keeps the old separate-line layout
        via base_stat(), so this default only matters if Layout1x is ever rendered
        through the new Painter.paint_rtcm())."""
        return SX, SY - 9

    def serial(self, state):
        """(font, text, (x, y)) for paintSerial6digit()."""
        return self.serial_font, state.serial, self.serial_pos

    def correction_pos(self, entry):
        # setRadioIcons(): x hugs the Logging icon, y is the common row for all sources
        row_y = min(DISPLAY_HEIGHT - (c["yOffset"] + c["height"]) for c in CORRECTION)
        return self.logging_x - ICON_GAP - entry["width"], row_y + entry["yOffset"]

    def broadcast_positions(self, entries):
        # paintBaseBroadcastIcons(): pack right-to-left from the Logging icon, bottom aligned
        positions, cell_right = [], self.logging_x
        zone_left = self.logging_x - sum(e["width"] + ICON_GAP for e in entries)
        for e in entries:
            cell_left = cell_right - e["width"] - ICON_GAP
            if cell_left < zone_left:
                break
            positions.append((cell_left, DISPLAY_HEIGHT - e["height"]))
            cell_right = cell_left
        return positions

    def transform(self, icons, state):
        return icons


class Layout2x(Layout1x):
    """Candidate 2x layout (epaper_update.md Section 6.3), rover screens only.

    Every icon pixel-doubled. Top band packs the present icons right-to-left from
    the battery; middle band crosshair / HPA / SIV / tilt; bottom band IP /
    correction / logging. Fonts stay 10x20."""

    name = "2x"
    scale = 2
    serial_pos = (0, 4)  # 20 px glyphs centered in the 28 px top band
    ip_y = 68
    logging_x = 184 - 2 * 9
    TOP_BAND = 28
    TOP_ORDER = ["battery", "mode", "arrow", "espnow", "wifi", "bt"]  # right-to-left

    def hpa_text(self, icon):
        return 32, 34

    def siv_text(self, icon, base=False):
        return 92 + 24 + 2, 34

    def correction_pos(self, entry):
        return self.logging_x - ICON_GAP - 2 * entry["width"], DISPLAY_HEIGHT - 28 + 2 * entry["yOffset"]

    def broadcast_positions(self, entries):
        positions, cell_right = [], self.logging_x
        for e in entries:
            cell_left = cell_right - 2 * e["width"] - ICON_GAP
            positions.append((cell_left, DISPLAY_HEIGHT - 2 * e["height"]))
            cell_right = cell_left
        return positions

    def transform(self, icons, state):
        for icon in icons:
            icon["scale"] = 2
        x_right = 184
        for slot in self.TOP_ORDER:
            members = [i for i in icons if i["slot"] == slot]
            if not members:
                continue
            slot_w = max(2 * i["width"] for i in members)
            for i in members:
                i["x"] = x_right - slot_w + (slot_w - 2 * i["width"]) // 2
                i["y"] = (self.TOP_BAND - 2 * i["height"]) // 2
            x_right -= slot_w + ICON_GAP
        for i in icons:
            if i["slot"] == "accuracy":
                i["x"], i["y"] = 0, 29
            elif i["slot"] == "siv":
                i["x"], i["y"] = self.siv_x(state), 29 + (30 - 2 * i["height"]) // 2
            elif i["slot"] == "tilt":
                i["x"], i["y"] = 154, 29
            elif i["slot"] == "logging":
                i["x"], i["y"] = self.logging_x, DISPLAY_HEIGHT - 2 * i["height"]
            # correction / broadcast icons were placed by correction_pos / broadcast_positions
        return icons

    def siv_x(self, state):
        return 92


def _colon_raise(font, text_y, row_center):
    """Pixels to lift ':' so its ink centers on the row (digits already sit centered)."""
    top, bottom = glyph_ink_rows(font, ":")
    return round(text_y + (top + bottom) / 2 - row_center)


def _ink_center_align(base_font, base_char, base_y, font, char):
    """y for `char` in `font` so its ink vertically centers on the same row as
    `base_char` in `base_font` drawn at `base_y` - used where a fallback font
    (e.g. the shrunk base "Cast:"/"BaseCast:" line) must still look aligned with
    the normal-size text next to it (Section 9 review)."""
    bt, bb = glyph_ink_rows(base_font, base_char)
    t, b = glyph_ink_rows(font, char)
    return round(base_y + (bt + bb) / 2 - (t + b) / 2)


def _text_ink_end(font, text):
    """Rightmost lit column of `text` set in `font`, starting at x=0."""
    p = Panel(rop=ROP_XOR)
    p.set_font(font)
    p.set_cursor(0, 0)
    p.print(text)
    cols = [x for x in range(p.width) if any(p.buf[y * p.width + x] for y in range(p.height))]
    return cols[-1] if cols else -1


class Layout2xV3(Layout2x):
    """v3: builds on the v2 2x layout.

    - Serial number (BT MAC + variant) and IP address drop to 8x16, vertically centered
      in the top / bottom bands.
    - HPA and SIV colons lifted so their ink centers on the middle row with the icons
      and digits (the 10x20 ':' otherwise sits 2 px low).
    - Crosshairs follow the firmware: thick band (CrossHairBold) = RTK Fix, thin
      double crosshair (CrossHairDual) = RTK Float.
    - Base screens: base SIV icon + count at the left of the middle band, two 10x20
      lines to its right: status (Casting / Xmitting / BaseCast) over RTCM:count, or
      Mean: over Time: during survey-in.
    """

    name = "2x_v3"
    TOP_CENTER = 13.5  # rows 0-27
    MIDDLE_CENTER = 43.5  # rows 29-58, the 30 px crosshair / tilt
    BOTTOM_CENTER = 73.5  # rows 60-87, the 28 px correction icons

    serial_font = "8X16"
    ip_font = "8X16"
    colon_raise = _colon_raise("10X20", 34, MIDDLE_CENTER)
    rtcm_x_adjust = 0

    BASE_TEXT_X = 64  # right of the base SIV column (icon 0-23, ':nn' 26-55)
    # The middle band is rows 28-59. Line 1 (Casting / Xmitting have descenders) inks
    # cell rows 3-19, line 2 (RTCM:nnn / Time:nn) inks rows 3-15. One blank row below
    # the descenders and one above the bottom-band broadcast icons (row 60).
    BASE_LINE_1_Y = 25  # ink rows 28-44
    BASE_LINE_2_Y = 43  # ink rows 46-58

    def __init__(self):
        top, bottom = glyph_ink_rows(self.serial_font, "0")
        self.serial_pos = (0, round(self.TOP_CENTER - (top + bottom) / 2))
        top, bottom = glyph_ink_rows(self.ip_font, "0")
        self.ip_y = round(self.BOTTOM_CENTER - (top + bottom) / 2)

    def siv_x(self, state):
        return 0 if state.base else 92

    def siv_text(self, icon, base=False):
        return (0 if base else 92) + 24 + 2, 34

    def base_stat(self, name):
        # Only Mean:/Time: (survey-in) use this two-line layout from v3 onward -
        # Casting/Xmitting/BaseCast/RTCM: moved to the single-line base_cast_pos()
        # in v4 (Section 6.4); kept mapped here too in case v3 is ever re-rendered.
        data_x = self.BASE_TEXT_X + 5 * 11  # after a 5-char label ("RTCM:", "Mean:", "Time:")
        line_1 = (self.BASE_TEXT_X, self.BASE_LINE_1_Y, "10X20", data_x, self.BASE_LINE_1_Y, "10X20")
        line_2 = (self.BASE_TEXT_X, self.BASE_LINE_2_Y, "10X20", data_x, self.BASE_LINE_2_Y, "10X20")
        return {"Mean:": line_1, "BaseCast": line_1, "Casting": line_1, "Xmitting": line_1,
                "Time:": line_2, "RTCM:": line_2}[name]

    def base_cast_pos(self, text_width, siv_end_x):
        return self.BASE_TEXT_X, self.BASE_LINE_1_Y


class Layout2xV4(Layout2xV3):
    """v4: builds on v3.

    - Top row icons have FIXED positions: each one appears or disappears in its own
      slot, nothing slides when another icon comes or goes. Left to right: BT, WiFi,
      ESP-NOW, arrow (down/up share it), dynamic model / base icon, battery.
    - Serial number: 10x20 while Bluetooth is not connected ("B4E706"). While it is
      connected the BT icon takes the place of the trailing model digits ("B4E7"),
      text still 10x20. serial_style="small" shows all six characters in 8x16 instead.
    - SIV colon moved 3 px left on rover and base screens.

    Slot x values are the left edge of the icon's ink (lit columns), so icons with
    blank edge columns still line up. The model/base slot is the full 30 px box
    (Automotive and Tractor use all 15 source columns) with the icon centered in it.
    """

    # IP address as the firmware draws it (displayFullIPAddress(), 184x88): 10x20 at y=68,
    # 11 px print() pitch - confirmed on hardware. Wider than v3's 8x16, so long IPs show
    # one or two characters fewer per ping-pong step.
    ip_font = "10X20"
    IP_Y = 68

    name = "2x_v4"
    siv_colon_dx = -2  # Firmware paintSIVText(): colon 2 px left of the text x
    # Firmware (printColonText() / paintSIVText()): the colon keeps its row and the digits
    # drop 3 px so the colon's dot midline bisects them - i.e. digits sit 3 px below the
    # raised colon's y (34 - colon_raise 2 + 3).
    digit_drop = 1
    SERIAL_LARGE = "10X20"
    SERIAL_SMALL = "8X16"
    # Large "B4E7" inks to x 41, small "B4E706" to x 50, large "B4E706" to x 63
    # bt moved 3px left (was 52) per Section 9 review.
    TOP_INK_X = {"bt": 49, "wifi": 67, "espnow": 81, "arrow": 98}
    MODE_RIGHT_EDGE = 143  # mode/base icon's ink right-aligns here, 2px before battery
    COMPACT_GAP = 2

    def __init__(self):
        super().__init__()
        self.serial_y = {}
        for font in (self.SERIAL_LARGE, self.SERIAL_SMALL):
            top, bottom = glyph_ink_rows(font, "0")
            self.serial_y[font] = round(self.TOP_CENTER - (top + bottom) / 2)
        self.ip_y = self.IP_Y

    def base_cast_font(self):
        return "10X20"  # same size as the SIV count next to it (Section 9 review)

    def base_cast_pos(self, text_width, siv_end_x):
        # Same row as the SIV colon/count (y=34, Section 9: "align horizontally with
        # the SIV text"), centered in the space between the SIV text and the right
        # edge of the panel. If it's wider than that space (e.g. "BaseCast:456"
        # doesn't fit at 10x20 in ~126px), left-align against the SIV text instead
        # of centering negative (which would overlap it).
        zone = 184 - siv_end_x
        x = siv_end_x + max(0, (zone - text_width) // 2)
        return x, 33  # Firmware paintRTCM(): colon on the base SIV colon's row

    def serial(self, state):
        if state.serial_style == "small":
            font, text = self.SERIAL_SMALL, state.serial
        elif state.bt_connected:
            font, text = self.SERIAL_LARGE, state.serial[:4]  # BT icon replaces the model digits
        else:
            font, text = self.SERIAL_LARGE, state.serial
        return font, text, (0, self.serial_y[font])

    def transform(self, icons, state):
        icons = super().transform(icons, state)  # scale, middle and bottom bands
        packed_slots = self._pack_compact(icons, state) if state.compact_top_row else set()
        for i in icons:
            slot = i["slot"]
            if slot in packed_slots:
                continue  # already positioned by _pack_compact()
            if slot in self.TOP_INK_X:
                i["x"] = self.TOP_INK_X[slot] - 2 * icon_ink_cols(i["bitmap"])[0]
            elif slot == "mode":
                # Right-align the icon's own ink to a fixed edge rather than reserving
                # a fixed-width box - narrower model icons (e.g. Portable) then leave
                # more clearance for the arrow slot than the widest ones (Automotive /
                # Tractor / BaseFixed) need.
                ink_last = icon_ink_cols(i["bitmap"])[1]
                i["x"] = self.MODE_RIGHT_EDGE - 2 * (ink_last + 1)
            elif slot == "battery":
                i["x"] = 184 - 2 * i["width"]
            elif slot == "correction":
                i["x"] += state.correction_x_nudge
        self._place_fixed_group(icons, state)
        return icons

    def _place_fixed_group(self, icons, state):
        """Firmware setRadioIcons() (184x88): the mode icon's cell sits iconGap (4) from the
        battery, rover dynamic models 2 px further right; the arrows' ink right edge sits
        iconGap left of the (un-nudged) mode cell, then 4 px right. Neither ever moves."""
        icon_gap, rover_mode_nudge, arrow_nudge = 4, 2, 4
        battery_left = 184 - 2 * PROPS["BatteryProperties"][0]["width"]
        mode_width = PROPS["BaseTemporaryProperties"]["width"] if state.base else PROPS["DynamicModel_1_Properties"]["width"]
        mode_x = battery_left - icon_gap - 2 * mode_width
        for i in icons:
            if i["slot"] == "mode":
                i["x"] = mode_x + (0 if state.base else rover_mode_nudge)
            elif i["slot"] == "arrow":
                ink_last = icon_ink_cols(i["bitmap"])[1]
                i["x"] = mode_x - icon_gap - 2 * (ink_last + 1) + arrow_nudge

    def _pack_compact(self, icons, state):
        """WiFi and ESP-NOW both active at once doesn't fit the normal fixed-slot
        spacing (Section 9: 12px overlap). For that case, pack icons left-to-right
        starting right after the serial number's own ink instead - there's room
        there precisely because a crowded top row is also the case where the serial
        gets shortened to "B4E7" (Section 9's guidance).

        The arrow is excluded on base screens: base's up/down arrow must stay in
        its normal fixed spot (next to the base type icon) on every base screen,
        compact or not (Section 9 review) - only wifi/espnow need repacking there.
        Rover's bt/wifi/espnow/arrow are all repacked together (only screen 00e
        needs this today).

        Returns the set of slots this positioned, so transform() knows not to also
        place them from TOP_INK_X.
        """
        font, text, _ = self.serial(state)
        x = _text_ink_end(font, text) + self.COMPACT_GAP
        slots = ("wifi", "espnow") if state.base else ("bt", "wifi", "espnow", "arrow")
        for slot in slots:
            members = [i for i in icons if i["slot"] == slot]
            if not members:
                continue
            for i in members:
                i["x"] = x - 2 * icon_ink_cols(i["bitmap"])[0]
            x += 2 * max(i["width"] for i in members) + self.COMPACT_GAP
        return set(slots)


# --------------------------------------------------------------------------------
# The paint path
# --------------------------------------------------------------------------------


class Painter:
    def __init__(self, state: State, layout=None):
        self.s = state
        self.L = layout or Layout1x()
        self.panel = Panel(rop=ROP_XOR)  # sticky XOR after the boot splash
        self.icons = []
        self.notes = []  # human-readable facts about this render (IP shuttle etc.)
        self.shuttle_len = 1  # printFittedText() ping-pong steps for the IP (1 = static)
        self.siv_text_end_x = 0  # set by paint_siv_text(); paint_rtcm() centers against it

    # ---- helpers ----------------------------------------------------------------

    def push(self, slot, prop, duty=DUTY_SOLID):
        icon = dict(prop)
        icon.update(slot=slot, duty=duty, scale=1)
        self.icons.append(icon)
        return icon

    def text_element(self, name, font, x, y, s):
        with self.panel.element(name):
            self.panel.set_font(font)
            self.panel.set_cursor(x, y)
            self.panel.print(s)

    def print_text_at(self, name, text, x, y, font, kerning=1):
        """printTextAt(): per-character placement at (fontWidth==8 ? 7 : fontWidth) + kerning."""
        p = self.panel
        with p.element(name):
            p.set_font(font)
            width = p.font["width"]
            if width == 8:
                width = 7
            for ch in text:
                p.set_cursor(x, y)
                p.print(ch)
                x += width + kerning

    # ---- displayUpdate() --------------------------------------------------------

    def display_update(self):
        st = self.s.system_state
        if st == "ROVER_NO_FIX":
            self.display_horizontal_accuracy("CrossHairProperties", DUTY_BLINK)
            self.rover_common()
        elif st == "ROVER_FIX":
            if self.s.ppp_converging:
                self.display_rtk_accuracy("CrossHairPppConvergedProperties", False)
            else:
                self.display_horizontal_accuracy("CrossHairProperties", DUTY_SOLID)
            self.rover_common()
        elif st == "ROVER_RTK_FLOAT":
            if self.s.ppp_converged:
                self.display_rtk_accuracy("CrossHairPppConvergedProperties", True)
            elif self.s.ppp_converging:
                self.display_rtk_accuracy("CrossHairPppConvergedProperties", False)
            else:
                self.display_rtk_accuracy("CrossHairDualProperties", False)
            self.rover_common()
        elif st == "ROVER_RTK_FIX":
            self.display_rtk_accuracy("CrossHairDualProperties", True)
            self.rover_common()
        elif st == "BASE_TEMP_SETTLE":
            self.display_horizontal_accuracy("CrossHairProperties", DUTY_BLINK)
            self.paint_logging()
            self.display_siv_vs_open_short()
            self.display_battery()
            self.display_full_ip_address()
            self.set_radio_icons()
        elif st == "BASE_TEMP_SURVEY_STARTED":
            self.paint_logging()
            self.display_battery()
            self.display_full_ip_address()
            self.set_radio_icons()
            self.paint_base_temp_survey_started()
            self.display_base_siv()
        elif st in ("BASE_TEMP_TRANSMITTING", "BASE_FIXED_TRANSMITTING"):
            self.paint_logging()
            self.display_battery()
            self.display_full_ip_address()
            self.set_radio_icons()
            self.display_base_siv()  # before paint_rtcm(): it needs siv_text_end_x
            self.paint_rtcm()
        else:
            raise ValueError(f"State {st} not ported yet")

        # "Now add the icons" - drawn after all text, through the same raster op
        for icon in self.L.transform(self.icons, self.s):
            if icon["duty"] == DUTY_SOLID or self.s.blink_on:
                with self.panel.element(f"{icon['slot']}:{icon['bitmap']}"):
                    self.panel.bitmap(icon["x"], icon["y"], icon["bitmap"], icon["width"], icon["height"], icon["scale"])
        if self.s.edge_frame:
            # drawFrame(): 1 px lines on the panel's outermost rows and columns
            with self.panel.element("edge frame"):
                for x in range(184):
                    self.panel.draw_pixel(x, 0, True)
                    self.panel.draw_pixel(x, 87, True)
                for y in range(88):
                    self.panel.draw_pixel(0, y, True)
                    self.panel.draw_pixel(183, y, True)
        return self.panel

    def rover_common(self):
        self.paint_logging()
        self.display_siv_vs_open_short()
        self.display_tilt_icon()
        self.display_battery()
        self.display_full_ip_address()
        self.set_radio_icons()

    # ---- accuracy ---------------------------------------------------------------

    def display_horizontal_accuracy(self, props_name, duty):
        icon = self.push("accuracy", PROPS[props_name], duty)
        self.paint_horizontal_accuracy(*self.L.hpa_text(icon))

    def display_rtk_accuracy(self, props_name, fixed):
        duty = DUTY_SOLID if fixed else DUTY_BLINK
        # On e-paper: Float = Dual, Fixed = Bold, PPP converging / converged (bold), all solid
        if props_name == "CrossHairDualProperties":
            if fixed:
                props_name = "CrossHairBoldProperties"
            duty = DUTY_SOLID
        if props_name == "CrossHairPppConvergedProperties":
            if fixed:
                props_name = "CrossHairPppConvergedBoldProperties"
            duty = DUTY_SOLID
        props_name = self.L.icon_swap.get(props_name, props_name)
        icon = self.push("accuracy", PROPS[props_name], duty)
        self.paint_horizontal_accuracy(*self.L.hpa_text(icon))

    def paint_horizontal_accuracy(self, x, y):
        hpa = self.s.hpa
        if hpa > 30.0:
            value = ">30m"
        elif hpa >= 10.0:
            value = arduino_float(hpa, 1)
        elif hpa >= 1.0:
            value = arduino_float(hpa, 2)
        else:
            value = "." + "%03d" % int(hpa * 1000)  # Remove leading zero
        p = self.panel
        with p.element("HPA"):
            p.set_font("10X20")
            p.set_cursor(x, y - self.L.colon_raise)
            p.print(":")
            p.set_cursor(x + p.font["width"] + 1, y + self.L.digit_drop)  # where print() leaves the cursor
            p.print(value)

    # ---- SIV / antenna / tilt ---------------------------------------------------

    def paint_siv_icon(self, props_name=None, duty=DUTY_SOLID):
        s = self.s
        if props_name is None:
            if s.ppp_capable and s.ppp_mode:
                props_name = "PppIconProperties"
            elif s.lband_rx:
                props_name = "LBandIconProperties"
            else:
                props_name = "SIVIconProperties"
            if s.base:
                duty = DUTY_SOLID
            elif not s.fixed:
                duty = DUTY_BLINK
        icon = self.push("siv", PROPS[props_name], duty)
        return self.L.siv_text(icon, s.base)

    def paint_siv_text(self, coords):
        s = self.s
        x, y = coords
        p = self.panel
        with p.element("SIV text"):
            p.set_font("10X20")
            p.set_cursor(x, y)
            siv = s.siv
            if siv > 99:
                p.print(">")
                siv = 99
            else:
                p.set_cursor(x + self.L.siv_colon_dx, y - self.L.colon_raise)
                p.print(":")
            x += 8
            if not s.base and not s.fixed:
                siv = 0
            p.set_cursor(x, y + self.L.digit_drop)  # nudgeAndPrintSIV(), non-64x48
            p.print(siv)
        self.siv_text_end_x = p.cursor_x  # base_cast_pos() centers against this

    def display_siv_vs_open_short(self):
        s = self.s
        if not s.supports_short_open:
            coords = self.paint_siv_icon(None, DUTY_SOLID)
        elif s.antenna_shorted:
            coords = self.paint_siv_icon("ShortIconProperties", DUTY_BLINK)
        elif s.antenna_open:
            coords = self.paint_siv_icon("OpenIconProperties", DUTY_BLINK)
        else:
            coords = self.paint_siv_icon(None, DUTY_SOLID)
        self.paint_siv_text(coords)

    def display_base_siv(self):
        self.paint_siv_text(self.paint_siv_icon("BaseSIVIconProperties", DUTY_SOLID))

    def display_tilt_icon(self):
        if self.s.tilt_correcting:
            self.push("tilt", PROPS["TiltIconProperties"])

    # ---- battery / logging ------------------------------------------------------

    def display_battery(self):
        fraction = max(0, min(3, self.s.battery_percent // 25))
        self.push("battery", PROPS["BatteryProperties"][fraction], DUTY_BLINK if fraction == 0 else DUTY_SOLID)

    def paint_logging(self):
        s = self.s
        frame = s.anim_frame if s.anim_frame is not None else (3 if s.logging else 0)
        table = {
            None: "PulseIconProperties",
            "STANDARD": "LoggingIconProperties",
            "PPP": "LoggingPPPIconProperties",
            "CUSTOM": "LoggingCustomIconProperties",
        }[s.logging]
        self.push("logging", PROPS[table][frame])

    # ---- IP address -------------------------------------------------------------

    def bottom_row_left_edge(self):
        """Leftmost x used by the bottom-right icon group (correction or broadcast
        icons, else the logging slot)."""
        s = self.s
        if s.rover and s.correction_source:
            return self.L.correction_pos(CORRECTION[CORRECTION_IDS.index(s.correction_source)])[0]
        if s.base and s.broadcasts:
            return min(x for x, _ in self.L.broadcast_positions(self.active_broadcasts()))
        return self.L.logging_x

    def display_full_ip_address(self):
        s = self.s
        if not (s.network_has_internet or s.wifi_softap) or not s.ip:
            return
        font = self.L.ip_font
        if s.ip_fit == "firmware":
            # Today: 8 px/char and only base broadcast icons are reserved
            zone = sum(e["width"] + ICON_GAP for e in self.active_broadcasts()) if s.base else 0
            max_width = self.L.logging_x - zone if self.L.logging_x > zone else 0
            char_width = 8
        else:
            # Fixed: real print() pitch, stop 2 px short of the bottom-right icons
            max_width = self.bottom_row_left_edge() - ICON_GAP
            char_width = FONTS[font]["width"] + 1
        self.print_fitted_text(s.ip, 0, self.L.ip_y, char_width, max_width, font)

    def print_fitted_text(self, text, x, y, char_width, max_width, font="10X20"):
        max_chars = max(1, max_width // char_width)
        if len(text) <= max_chars:
            self.notes.append(f"IP '{text}' fits ({len(text)} <= {max_chars} chars), static")
            self.text_element("IP", font, x, y, text)
            return
        extras = len(text) - max_chars
        shuttle = [0] + list(range(0, extras + 1)) + [extras] + list(range(extras - 1, 0, -1))
        self.shuttle_len = len(shuttle)
        start = shuttle[self.s.shuttle_step % len(shuttle)]
        window = text[start : start + max_chars]
        self.notes.append(
            f"IP '{text}' ping-pongs: {max_chars}-char window, {len(shuttle)} steps "
            f"= {2 * len(shuttle)} s cycle on e-paper; step {self.s.shuttle_step % len(shuttle)} shows '{window}'"
        )
        self.text_element("IP", font, x, y, window)

    def shuttle_steps(self):
        """Number of distinct ping-pong steps for this state's IP (1 if static)."""
        probe = Painter(self.s, self.L)
        probe.display_full_ip_address()
        return probe.shuttle_len

    # ---- top row / radio icons --------------------------------------------------

    def paint_serial_6digit(self):
        font, text, (x, y) = self.L.serial(self.s)
        self.text_element("serial", font, x, y, text)

    def wifi_by_rssi(self):
        rssi = self.s.wifi_rssi
        level = 3 if rssi >= -40 else 2 if rssi >= -60 else 1 if rssi >= -80 else 0
        return PROPS[f"WiFiSymbol{level}184x88"]

    def active_broadcasts(self):
        return [BROADCAST[BCAST_IDS.index(b)] for b in BCAST_PRIORITY if b in self.s.broadcasts]

    def set_radio_icons(self):
        s = self.s
        self.paint_serial_6digit()

        if s.bt_connected:
            self.push("bt", PROPS["BTSymbol184x88"])

        if s.wifi_station and s.wifi_internet:
            self.push("wifi", self.wifi_by_rssi())
        elif s.wifi_station and not s.wifi_internet:
            self.push("wifi", PROPS["WiFiSymbolNC184x88"])  # no blink on e-paper
        elif s.wifi_softap:
            self.push("wifi", self.wifi_by_rssi())

        if s.espnow_paired:
            r = s.espnow_rssi
            if r > -255:
                level = 3 if r >= -40 else 2 if r >= -60 else 1 if r >= -80 else 0
                self.push("espnow", PROPS[f"ESPNowSymbol{level}184x88"])

        # Every source pushes its own arrow - two sources push the same arrow twice,
        # which XOR-cancels (see epaper_update.md Section 7)
        if s.bt_connected:
            if s.bt_rtcm_in:
                self.push("arrow", PROPS["DownloadArrow184x88"])
            if s.bt_rtcm_out:
                self.push("arrow", PROPS["UploadArrow184x88"])
        if s.espnow_paired:
            if s.espnow_rtcm_in:
                self.push("arrow", PROPS["DownloadArrow184x88"])
            if s.espnow_rtcm_out:
                self.push("arrow", PROPS["UploadArrow184x88"])
        if s.usb_rtcm_in:
            self.push("arrow", PROPS["DownloadArrow184x88"])
        if s.network_has_internet:
            if s.net_rtcm_in:
                self.push("arrow", PROPS["DownloadArrow184x88"])
            if s.mqtt_rx:
                self.push("arrow", PROPS["DownloadArrow184x88"])
            if s.net_rtcm_out:
                self.push("arrow", PROPS["UploadArrow184x88"])

        st = s.system_state
        if st in ROVER_STATES:
            if s.dynamic_model:  # paintDynamicModel()
                self.push("mode", PROPS[s.dynamic_model])
        elif st in ("BASE_TEMP_SETTLE", "BASE_TEMP_SURVEY_STARTED", "BASE_TEMP_TRANSMITTING"):
            self.push("mode", PROPS["BaseTemporaryProperties"])
        elif st == "BASE_FIXED_TRANSMITTING":
            self.push("mode", PROPS["BaseFixedProperties"])

        if s.rover and s.correction_source:
            entry = CORRECTION[CORRECTION_IDS.index(s.correction_source)]
            x, y = self.L.correction_pos(entry)
            self.push("correction", {**entry, "x": x, "y": y})
        elif s.base:
            entries = self.active_broadcasts()
            for entry, (x, y) in zip(entries, self.L.broadcast_positions(entries)):
                self.push("broadcast", {**entry, "x": x, "y": y})

    # ---- base -------------------------------------------------------------------

    def paint_base_temp_survey_started(self):
        s = self.s
        lx, ly, lfont, dx, dy, dfont = self.L.base_stat("Mean:")
        self.text_element("Mean label", lfont, lx, ly, "Mean:")
        mean = arduino_float(s.survey_mean, 2) if s.survey_mean < 10.0 else ">10"
        self.text_element("Mean value", dfont, dx, dy, mean)

        lx, ly, lfont, dx, dy, dfont = self.L.base_stat("Time:")
        if s.supports_short_open and s.antenna_shorted:
            self.paint_siv_icon("ShortIconProperties", DUTY_BLINK)
        elif s.supports_short_open and s.antenna_open:
            self.paint_siv_icon("OpenIconProperties", DUTY_BLINK)
        else:
            self.text_element("Time label", lfont, lx, ly, "Time:")
        self.text_element("Time value", dfont, dx, dy, s.survey_time if s.survey_time < 1000 else "0")

    def paint_rtcm(self):
        s = self.s
        if s.supports_short_open and s.antenna_shorted:
            self.paint_siv_icon("ShortIconProperties", DUTY_BLINK)
        elif s.supports_short_open and s.antenna_open:
            self.paint_siv_icon("OpenIconProperties", DUTY_BLINK)

        # No separate "RTCM:" label - the status word and the count are one string
        # ("Cast:123" / "Xmit:123" / "BaseCast:123"), printed on the SIV row instead
        # of its own line (epaper_update.md Section 6.4). The survey-in screen
        # (Mean: / Time:) is untouched - see paint_base_temp_survey_started().
        if s.base_caster_override:
            prefix = "B-Cast"  # Shortened from BaseCast so it fits in 10x20
        elif s.ntrip_server_online:
            prefix = "Cast"
        else:
            prefix = "Xmit"
        text = f"{prefix}:{s.rtcm_packets}"
        font = self.L.base_cast_font()
        width = _text_ink_end(font, text) + 1
        zone = 184 - self.siv_text_end_x
        if width > zone:
            # Doesn't fit at the normal size (e.g. "BaseCast:456", 12 chars, needs
            # more than the ~128px left after the SIV column) - shrink the font
            # rather than let it wrap into the IP line below (Section 9's crowding
            # rule: shorten the MAC first, then shrink the font).
            font = "8X16"
            width = _text_ink_end(font, text) + 1
        x, y = self.L.base_cast_pos(width, self.siv_text_end_x)
        if font != "10X20":
            # Shrunk fallback: re-align its ink center to the 10x20 SIV count's
            # ink center on the same row, rather than sharing its raw y (a smaller
            # font's ink sits higher in the same font-cell, reading as "floating").
            y = _ink_center_align("10X20", "2", y, font, "B")
        if self.L.digit_drop:
            # printColonText(): colon on its row, everything else dropped under it
            drop = 3 if font == "10X20" else 2
            p = self.panel
            with p.element("cast status"):
                p.set_font(font)
                for ch in text:
                    p.set_cursor(x, y if ch == ":" else y + drop)
                    p.print(ch)
                    x += FONTS[font]["width"] + 1
        else:
            self.text_element("cast status", font, x, y, text)


# displayAutoSizeEpFonts[] / displayAutoSizeEpFontLineSpacing[] (Display.ino:3723-3725)
MESSAGE_EP_FONTS = [("5X7", 8), ("8X16", 16), ("10X20", 20)]


def _display_message(painter, message, top_reserve=0):
    """displayMessageReserveTop(), DISPLAY_184x88 branch."""
    kerning = 1
    x_pixels, y_pixels = 184, 88
    if top_reserve > y_pixels:
        top_reserve = 0
    drawable = y_pixels - top_reserve

    # strtok() skips empty tokens: '\n' separated lines win, else split on spaces
    lines = [t for t in message.split("\n") if t]
    if len(lines) <= 1:
        lines = [t for t in message.split(" ") if t]
    if not lines:
        painter.notes.append("numLines is zero - nothing drawn")
        return
    longest = max(len(line) for line in lines)

    chosen = None
    for font, spacing in reversed(MESSAGE_EP_FONTS):
        if drawable >= len(lines) * spacing and x_pixels >= longest * (FONTS[font]["width"] + kerning):
            chosen = (font, spacing)
            break
    if chosen is None:
        painter.notes.append("no suitable font found - nothing drawn")
        return
    font, spacing = chosen
    painter.notes.append(f"font {font}, {len(lines)} line(s)")

    y = top_reserve + drawable // 2 - spacing // 2
    for _ in range(1, len(lines)):
        y -= spacing // 2
    y &= 0xFF  # uint8_t
    for n, line in enumerate(lines):
        _print_text_center(painter, f"line {n + 1}", line, y, font, kerning)
        y = (y + spacing) & 0xFF


def _print_text_center(painter, name, text, y, font, kerning):
    """printTextCenter() without highlight: per-character placement from a centered start."""
    width = FONTS[font]["width"]
    if width == 8:
        width = 7  # 8x16, but widest character is only 7 pixels
    text_width = (len(text) * (width + kerning)) & 0xFF  # uint8_t
    x = max(0, 184 // 2 - text_width // 2)
    painter.print_text_at(name, text, x, y, font, kerning)


def render_message(message, top_reserve=0):
    """A full-screen displayMessage() screen (erased panel, auto-sized centered text)."""
    painter = Painter(State(system_state="MESSAGE"))
    _display_message(painter, message, top_reserve)
    return painter.panel, painter


LOGO_SVG_ASPECT = 6229.19 / 894.08  # SparkPNT-Black.svg viewBox width/height


def render_powered_off(model_text):
    """The powered-off screen: SparkPNT logo + model name, held on the panel with
    zero power (e-paper is bistable) - distinct from displaySplash() (boot) and
    displayShutdown()'s plain "Shutting Down..." message.

    Per review: logo nearly full panel width, vertically centered in the top half
    (rows 0-43); model name in the largest font available (31x48, upper-case),
    vertically centered in the bottom half (rows 44-87).

    The logo itself is NOT drawn into the 1-bit `panel` here - it's vector art
    (`assets/SparkPNT-Black.svg`) rasterized with anti-aliasing by
    `render_screens.py` (needs svglib/reportlab, which `display_port.py` doesn't
    otherwise depend on) and composited onto the rendered PNG afterward. This
    function only reserves and returns the logo's box (native panel pixels) via
    `painter.logo_box` so the caller knows where to paste it.
    """
    painter = Painter(State(system_state="MESSAGE"))
    panel = painter.panel

    logo_w = 170  # "nearly the width of the screen" (184), small side margin
    logo_h = round(logo_w / LOGO_SVG_ASPECT)
    logo_x, logo_y = (184 - logo_w) // 2, (44 - logo_h) // 2
    painter.logo_box = (logo_x, logo_y, logo_w, logo_h)

    font = "31X48"
    top, bottom = glyph_ink_rows(font, model_text[0])
    text_y = round(44 + 44 / 2 - (top + bottom) / 2 - top)
    _print_text_center(painter, "model", model_text, text_y, font, kerning=1)

    painter.notes.append(f"powered-off: logo {logo_w}x{logo_h} (SVG, anti-aliased) in "
                          f"top half, '{model_text}' in {font} centered in bottom half")
    return panel, painter


def render_boot_logo():
    """paintBootLogo184x88(): the powered-off screen's 170x24 logo bitmap, alone, centered."""
    painter = Painter(State(system_state="MESSAGE"))
    icon = ICONS["SparkPNT_PoweredOff_Logo"]
    x, y = (184 - icon["width"]) // 2, (88 - icon["height"]) // 2
    with painter.panel.element("boot logo"):
        painter.panel.bitmap(x, y, "SparkPNT_PoweredOff_Logo")
    painter.notes.append(f"boot logo: {icon['width']}x{icon['height']} firmware bitmap at ({x}, {y})")
    return painter.panel, painter


def render_boot_info(model, firmware):
    """paintBootInfo184x88(): SparkPNT logo bitmap, model (31x48, else 10x20), firmware
    version (8x16, else 5x7), centered, with 3-4 px of space above, between and below."""
    painter = Painter(State(system_state="MESSAGE"))
    icon = ICONS["SparkPNT_PoweredOff_Logo"]
    with painter.panel.element("logo"):
        painter.panel.bitmap((184 - icon["width"]) // 2, 3, "SparkPNT_PoweredOff_Logo")
    if len(model) * (FONTS["31X48"]["width"] + 1) - 1 <= 184:
        _print_text_center(painter, "model", model, 31, "31X48", kerning=1)
    else:
        _print_text_center(painter, "model", model, 40, "10X20", kerning=1)
    if len(firmware) * (FONTS["8X16"]["width"] + 1) - 1 <= 184:
        _print_text_center(painter, "firmware", firmware, 70, "8X16", kerning=1)
    else:
        _print_text_center(painter, "firmware", firmware, 74, "5X7", kerning=1)
    return painter.panel, painter


def render_powered_off_firmware(model):
    """paintPoweredOff184x88() as the firmware draws it: the 1-bit logo bitmap at y=8 and the
    model in 31x48 at y=48 (M53 is the anti-aliased SVG mockup of the same screen)."""
    painter = Painter(State(system_state="MESSAGE"))
    icon = ICONS["SparkPNT_PoweredOff_Logo"]
    with painter.panel.element("logo"):
        painter.panel.bitmap((184 - icon["width"]) // 2, 8, "SparkPNT_PoweredOff_Logo")
    _print_text_center(painter, "model", model, 48, "31X48", kerning=1)
    return painter.panel, painter


def render(state: State, layout=None):
    painter = Painter(state, layout)
    panel = painter.display_update()
    return panel, painter
