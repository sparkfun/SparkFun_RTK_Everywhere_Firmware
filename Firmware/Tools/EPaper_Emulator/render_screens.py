######################################################################################
#
# render_screens.py
#
# Renders the rover and base screens from epaper_update.md Section 5 through the
# Display.ino port (display_port.py) in the current 2x layout (v4), plus every
# full-screen displayMessage() screen the Facet FP can show. Each run writes into
# its own fresh, auto-numbered folder (output/v5, output/v6, ...) rather than
# overwriting the previous run, so old renders stay around for comparison:
#
#   output/vN/screens/        one PNG per rover / base screen
#   output/vN/messages/       one PNG per message screen
#   output/vN/debug/          rover / base renders with element boxes and names overlaid
#   output/vN/sheet.png       rover / base screens on one labeled sheet
#   output/vN/sheet_messages.png   message screens on one labeled sheet
#   output/vN/screen00_*      extras for screen 0: every IP ping-pong step, true
#                             physical size
#   output/vN/overlap_report.md   lit-pixel collisions and touching elements
#
# Use: python render_screens.py   (run extract_icons.py / extract_fonts.py first
#                                  whenever icons.h or the library changes)
#
######################################################################################

import re
from dataclasses import replace
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

from display_port import Layout2xV4, State, render, render_boot_info, render_boot_logo, render_message, render_powered_off, render_powered_off_firmware
from epaper_emulator import pixel_size_for_ppi

LOGO_SVG = Path(__file__).parent / "assets" / "SparkPNT-Black.svg"


def render_logo_grayscale(svg_path, target_width, oversample=6):
    """Rasterizes the vector SparkPNT logo with real anti-aliasing (via
    svglib/reportlab - no native cairo needed) at `oversample`x the target size,
    then LANCZOS-downsamples to it, so the edges stay smooth instead of the
    blocky nearest-neighbor look `bitmap_scaled()` gives a bitmap icon blown up
    this large. Returns an RGBA image: black ink at full alpha, background at
    zero alpha, ready to `paste(..., mask=img)` onto the panel render."""
    from svglib.svglib import svg2rlg
    from reportlab.graphics import renderPM

    drawing = svg2rlg(str(svg_path))
    aspect = drawing.height / drawing.width
    hi_w = target_width * oversample
    hi_h = round(hi_w * aspect)
    dpi = 72 * oversample * target_width / drawing.width
    hi = renderPM.drawToPIL(drawing, dpi=dpi, bg=0xFFFFFF).convert("L")
    if hi.size != (hi_w, hi_h):
        hi = hi.resize((hi_w, hi_h), Image.LANCZOS)
    small = hi.resize((target_width, round(target_width * aspect)), Image.LANCZOS)
    alpha = ImageOps.invert(small)  # dark ink -> high alpha; white bg -> transparent
    logo = Image.new("RGBA", small.size, (0, 0, 0, 255))
    logo.putalpha(alpha)
    return logo


def next_output_dir():
    """output/v1, v2, ... - each run gets its own folder; existing ones are never
    reused or overwritten."""
    root = Path(__file__).parent / "output"
    existing = [int(m.group(1)) for p in root.glob("v*") if (m := re.fullmatch(r"v(\d+)", p.name))]
    return root / f"v{max(existing, default=0) + 1}"


OUT_DIR = next_output_dir()
LAYOUT = Layout2xV4()
INSPECT_SCALE = 6
ASUS_VE278_PPI = 81.6  # 27" 1920x1080, unverified against an official spec

LONG_IP = "192.168.252.101"  # longest IPv4 form - forces the ping-pong


def rover(system_state="ROVER_RTK_FIX", **kw):
    return State(system_state=system_state, **kw)


# Screen 0: every rover element on at once, worst-case text (Section 4.4)
SCREEN_0 = rover(
    bt_connected=True,
    wifi_station=True,
    wifi_internet=True,
    net_rtcm_in=True,
    hpa=0.008,
    siv=42,
    tilt_correcting=True,
    ip=LONG_IP,
    correction_source="TCP",
)

# (id, name, state)
SCREENS = [
    ("00a", "rover_all_icons_logging_standard", replace(SCREEN_0, logging="STANDARD")),
    ("00b", "rover_all_icons_logging_ppp", replace(SCREEN_0, logging="PPP")),
    ("00c", "rover_all_icons_logging_custom", replace(SCREEN_0, logging="CUSTOM")),
    ("00d", "rover_all_icons_no_sd_pulse", replace(SCREEN_0, logging=None)),
    # Small 8x16 serial with every top-row icon on: BT, WiFi, ESP-NOW, download, model, battery
    # Serial dropped to "B4E7" (no "06") - Section 9 guidance: shorten the MAC first
    # when the top row is crowded.
    ("00e", "rover_small_serial_all_top_icons", replace(SCREEN_0, serial_style="small", serial="B4E7",
        espnow_paired=True, espnow_rssi=-50, logging="STANDARD", compact_top_row=True)),
    ("01", "rover_ntrip_wifi_bt_fixed", rover(bt_connected=True, wifi_station=True, wifi_internet=True, net_rtcm_in=True,
        hpa=0.014, siv=24, ip="192.168.1.42", correction_source="TCP")),
    ("02", "rover_ntrip_wifi_bt_float", rover("ROVER_RTK_FLOAT", bt_connected=True, wifi_station=True, wifi_internet=True,
        net_rtcm_in=True, hpa=0.42, siv=18, ip="192.168.1.42", correction_source="TCP", logging="STANDARD")),
    # Correction-source icon is BT_Symbol too (CORRECTION_IDS[BLUETOOTH]) - nudged
    # 2px further left of the logging icon for clearance (Section 9).
    ("03", "rover_fixed_tilt_bt_corrections", rover(bt_connected=True, bt_rtcm_in=True, hpa=0.011, siv=31,
        tilt_correcting=True, correction_source="BLUETOOTH", logging="STANDARD", correction_x_nudge=-2)),
    ("04", "rover_wifi_3dfix", rover("ROVER_FIX", wifi_station=True, wifi_internet=True, wifi_rssi=-55, hpa=1.8, siv=14,
        ip="192.168.1.42", dynamic_model="DynamicModel_4_Properties")),  # Automotive
    ("05", "rover_bt_only_autonomous", rover("ROVER_FIX", bt_connected=True, hpa=3.2, siv=9, battery_percent=30,
        dynamic_model="DynamicModel_9_Properties")),  # Wrist
    ("06", "rover_ppp_converging", rover("ROVER_FIX", bt_connected=True, ppp_capable=True, ppp_mode=True,
        ppp_converging=True, hpa=0.35, siv=30, correction_source="PPP_HAS_B2B", logging="PPP",
        dynamic_model="DynamicModel_2_Properties")),  # Stationary
    ("07", "rover_ppp_converged", rover("ROVER_RTK_FLOAT", bt_connected=True, ppp_capable=True, ppp_mode=True,
        ppp_converged=True, hpa=0.052, siv=33, correction_source="PPP_HAS_B2B", logging="PPP",
        dynamic_model="DynamicModel_3_Properties")),  # Pedestrian
    ("08", "rover_espnow_corrections", rover(espnow_paired=True, espnow_rssi=-50, espnow_rtcm_in=True, hpa=0.021, siv=22,
        correction_source="ESPNOW", dynamic_model="DynamicModel_11_Properties")),  # Mower
    ("09", "rover_lora_corrections", rover(bt_connected=True, hpa=0.018, siv=27, correction_source="RADIO_LORA",
        logging="CUSTOM", dynamic_model="DynamicModel_Tractor_Props")),
    ("10", "rover_logging_low_battery_usb", rover("ROVER_RTK_FLOAT", usb_rtcm_in=True, hpa=0.25, siv=20, battery_percent=10,
        correction_source="USB", logging="STANDARD", anim_frame=1, dynamic_model="DynamicModel_10_Properties")),  # Bike
    ("11", "rover_pointperfect_ip", rover(wifi_station=True, wifi_internet=True, mqtt_rx=True, hpa=0.016, siv=26,
        ip="10.0.0.12", correction_source="IP", dynamic_model="DynamicModel_12_Properties")),  # E-Scooter
    ("12", "rover_no_fix_searching", rover("ROVER_NO_FIX", hpa=35.0, siv=3, dynamic_model="DynamicModel_6_Properties")),  # Airborne <1g
    ("13", "rover_antenna_short", rover("ROVER_NO_FIX", wifi_station=True, wifi_internet=True, hpa=35.0, siv=0,
        supports_short_open=True, antenna_shorted=True, ip="192.168.1.42", correction_source="TCP",
        dynamic_model="DynamicModel_7_Properties")),  # Airborne <2g
    ("14", "base_fixed_ntrip_server_wifi", State("BASE_FIXED_TRANSMITTING", wifi_station=True, wifi_internet=True,
        net_rtcm_out=True, ntrip_server_online=True, broadcasts=["NTRIP_SERVER"], rtcm_packets=999, siv=28, ip=LONG_IP)),
    # Serial dropped to "B4E7" and WiFi/ESP-NOW icons re-spaced (Section 9) to clear
    # the collision both radios being active at once used to cause.
    ("15", "base_fixed_espnow_lora", State("BASE_FIXED_TRANSMITTING", wifi_station=True, wifi_internet=True,
        espnow_paired=True, espnow_rtcm_out=True, broadcasts=["ESPNOW", "RADIO_LORA"], rtcm_packets=87, siv=25, ip=LONG_IP,
        logging="STANDARD", serial="B4E7", compact_top_row=True)),
    ("16", "base_survey_in", State("BASE_TEMP_SURVEY_STARTED", wifi_station=True, wifi_internet=True, survey_mean=1.23,
        survey_time=47, siv=24, ip=LONG_IP)),
    ("17", "base_temp_caster", State("BASE_TEMP_TRANSMITTING", wifi_station=True, wifi_internet=True, net_rtcm_out=True,
        base_caster_override=True, broadcasts=["NTRIP_CASTER"], rtcm_packets=456, siv=26, ip=LONG_IP, logging="CUSTOM")),
    # Long IP and three outgoing correction icons (LoRa, ESP-NOW, NTRIP) next to the logging icon.
    # espnow_rtcm_out dropped: it pushed the same UploadArrow bitmap as net_rtcm_out
    # at the same spot, and two pushes of the same icon XOR-cancel to nothing
    # (Section 7) - NTRIP's upload arrow alone represents "transmitting" here.
    # Serial dropped to "B4E7" and WiFi/ESP-NOW re-spaced, same as screen 15.
    ("18", "base_long_ip_lora_espnow_ntrip", State("BASE_FIXED_TRANSMITTING", wifi_station=True, wifi_internet=True,
        net_rtcm_out=True, ntrip_server_online=True, espnow_paired=True, espnow_rtcm_out=False,
        broadcasts=["RADIO_LORA", "ESPNOW", "NTRIP_SERVER"], rtcm_packets=999, siv=31, ip=LONG_IP, logging="STANDARD",
        serial="B4E7", compact_top_row=True)),
    # Screen 00a plus a frame on the outermost pixels - where the panel edge falls vs. the icons
    ("19", "rover_edge_frame", replace(SCREEN_0, logging="STANDARD", edge_frame=True)),
]

# Full-screen displayMessage() screens the Facet FP can reach (Display.ino), grouped by
# purpose. Ethernet / NTP / ZED-F9R / L-Band-only messages are left out.
MESSAGES = [
    # Mode changes
    ("M01", "rover_start", "Rover\n "),
    ("M02", "rover_started", "Rover Started"),
    ("M03", "rover_failed", "Rover Failed"),
    ("M04", "base_start", "Base\n "),
    ("M05", "base_started", "Base Started"),
    ("M06", "base_failed", "Base Failed"),
    ("M07", "basecast_start", "BaseCast\n "),
    ("M08", "survey_start", "Survey\n "),
    ("M09", "survey_started", "Survey Started"),
    ("M10", "web_config", "Web Config"),
    ("M11", "serial_config", "Serial Config"),
    ("M12", "loading_profile", "Loading Survey"),
    # Updates - displayFirmwareUpdateProgress() and paintGenericUpdate()
    ("M13", "system_update_47", "System\nUpdate\n47%"),
    ("M14", "gnss_update_34", "GNSS\nUpdate\n34%"),
    ("M15", "lora_update_80", "LoRa\nUpdate\n80%"),
    ("M16", "gnss_update_button_exit", "GNSS\nUpdate\nButton\nTo Exit"),
    ("M17", "lora_rx_direct", "LoRa\nRX Direct\nButton\nTo Exit"),
    ("M18", "forced_update", "Forced Update"),
    # ESP-NOW
    ("M19", "espnow_pairing", "ESP-NOW Pairing"),
    ("M20", "espnow_paired", "ESP-NOW Paired"),
    ("M21", "espnow_timeout", "ESP-NOW Timeout"),
    # GNSS / tilt detection
    ("M22", "autodetecting_gnss", "Autodetecting GNSS"),
    ("M23", "autodetect_failed", "Autodetect Failed"),
    ("M24", "gnss_booting", "GNSS Booting"),
    ("M25", "gnss_failed", "GNSS Failed"),
    ("M26", "autodetecting_tilt", "Autodetecting Tilt"),
    ("M27", "no_tilt", "No Tilt"),
    # Network / NTRIP
    ("M28", "wifi_connect", "WiFi Connect"),
    ("M29", "no_wifi", "No WiFi"),
    ("M30", "no_network", "No Network"),
    ("M31", "no_ssids", "No SSIDs"),
    ("M32", "ntrip_client_failed", "NTRIP\nClient\nFailed\nNo WiFi"),
    ("M33", "ntrip_server_failed", "NTRIP\nServer\nFailed\nNo WiFi"),
    # PointPerfect
    ("M34", "getting_keys", "Getting Keys"),
    ("M35", "getting_creds", "Getting Creds"),
    ("M36", "keys_updated", "Keys Updated"),
    ("M37", "keys_expired", "Keys Expired"),
    ("M38", "account_expired", "Account Expired"),
    ("M39", "not_listed", "Not Listed"),
    ("M40", "already_registered", "Already Register"),
    ("M41", "pp_update_failed", "PP\nUpdate\nFailed\nNo Network"),
    ("M42", "ztp_failed", "Failed ZTP ID:\n3C8A1F2DB4E706"),  # full 14-char MAC, one line
    # Logging / marking
    ("M43", "format_sd", "Format\nSD Card"),
    ("M44", "no_logging", "No Logging"),
    ("M45", "marked", "Marked"),
    ("M46", "mark_failure", "Mark Failure"),
    ("M47", "not_marked", "Not Marked"),
    ("M48", "event_marked", "Event Marked"),
    # System
    ("M49", "rtc_wait", "RTC Wait"),
    ("M50", "factory_reset", "Factory Reset"),
    ("M51", "buffer_size", "Fix GNSS\nHandler\nBuffer Sz"),
    ("M52", "shutting_down", "Shutting Down..."),
]


def build_sheet(rendered, cols=3, pad=14, label_h=22, margin=24):
    cell_w = max(img.width for _, img in rendered)
    cell_h = max(img.height for _, img in rendered)
    rows = (len(rendered) + cols - 1) // cols
    sheet = Image.new("RGB", (margin * 2 + cols * cell_w + (cols - 1) * pad,
                              margin * 2 + rows * (cell_h + label_h) + (rows - 1) * pad), (255, 255, 255))
    draw = ImageDraw.Draw(sheet)
    for i, (label, img) in enumerate(rendered):
        r, c = divmod(i, cols)
        x = margin + c * (cell_w + pad)
        y = margin + r * (cell_h + label_h + pad)
        sheet.paste(img, (x, y))
        draw.text((x, y + cell_h + 4), label, fill=(0, 0, 0))
    return sheet


def write_screen(sid, name, panel):
    (OUT_DIR / "screens").mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "debug").mkdir(parents=True, exist_ok=True)
    img = panel.to_image(INSPECT_SCALE)
    img.save(OUT_DIR / "screens" / f"{sid}_{name}.png")
    panel.debug_image(INSPECT_SCALE).save(OUT_DIR / "debug" / f"{sid}_{name}.png")
    return img


def pingpong_strip(state, path):
    """Every ping-pong step of the IP, as the e-paper would show them 2 s apart."""
    steps = render(state, LAYOUT)[1].shuttle_steps()
    frames = []
    for step in range(steps):
        panel, _ = render(replace(state, shuttle_step=step), LAYOUT)
        frames.append((f"step {step} (t = {2 * step} s)", panel.to_image(4)))
    build_sheet(frames, cols=2).save(path)
    return steps


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    report = ["# Overlap report (v4)", "",
              "Lit-pixel collisions between elements (the panel draws in XOR after the splash, so "
              "every colliding pixel renders as a hole), and elements that touch with no blank "
              "pixel between them.", ""]
    rendered = []
    for sid, name, state in SCREENS:
        panel, painter = render(state, LAYOUT)
        rendered.append((f"{sid} {name}", write_screen(sid, name, panel)))
        lines = [f"- overlap `{a}` x `{b}`: {n} px" for a, b, n in panel.overlaps()]
        lines += [f"- touching `{a}` x `{b}`: {n} px" for a, b, n in panel.touching()]
        lines = lines or ["- none"]
        report += [f"**{sid} {name}**", ""] + lines + [f"- note: {n}" for n in painter.notes] + [""]
    # Boot screens (DisplayTest B1/B2) - not status screens, rendered on their own
    for sid, name, (panel, painter) in (
        ("B1", "boot_logo", render_boot_logo()),
        ("B2", "boot_screen", render_boot_info("FPL", "d99.99-Sep 24 2026")),  # Matches the test unit
        ("B3", "powered_off", render_powered_off_firmware("FPL")),
        ("B4", "shutting_down", render_message("Shutting Down...")),
    ):
        rendered.append((f"{sid} {name}", write_screen(sid, name, panel)))
        report += [f"**{sid} {name}**", ""] + [f"- note: {n}" for n in painter.notes] + [""]
    build_sheet(rendered).save(OUT_DIR / "sheet.png")
    print(f"{LAYOUT.name}: {len(rendered)} rover / base screens")

    (OUT_DIR / "messages").mkdir(parents=True, exist_ok=True)
    messages = []
    report += ["## Message screens", ""]
    for mid, name, text in MESSAGES:
        panel, painter = render_message(text)
        img = panel.to_image(INSPECT_SCALE)
        img.save(OUT_DIR / "messages" / f"{mid}_{name}.png")
        messages.append((f"{mid} {name}", img))
        report.append(f"- {mid} {name}: " + "; ".join(painter.notes))

    # Powered-off state: logo + model name, held with zero power (e-paper is
    # bistable) - not a displayMessage() screen, so rendered separately. The logo
    # is real vector art, anti-aliased and composited on top of the 1-bit panel
    # render (which only has the "FPM" text baked in - see render_powered_off()).
    panel, painter = render_powered_off("FPM")
    # Composite at NATIVE 184x88 first: the "FPM" text is already pixel-exact
    # bitmap font data (no resampling loss, same as real firmware), and the logo
    # is rendered straight to its real target width (170 native px) - rasterized
    # from the SVG at high internal oversample then LANCZOS-downsampled to that
    # size, so it's properly anti-aliased AT the panel's actual dot count rather
    # than a supersampled preview of detail 184x88 dots could never resolve. Only
    # then upscale (NEAREST) for inspection, so both text and logo blow up into
    # the same uniform per-dot blocks - what the panel's few-level grayscale mode
    # (GDEM0097T61 datasheet, Section 4.3) would actually show.
    native = panel.to_image(1).convert("RGBA")
    logo_x, logo_y, logo_w, _ = painter.logo_box
    logo = render_logo_grayscale(LOGO_SVG, logo_w)
    native.paste(logo, (logo_x, logo_y), logo)
    img = native.convert("RGB").resize((184 * INSPECT_SCALE, 88 * INSPECT_SCALE), Image.NEAREST)
    img.save(OUT_DIR / "messages" / "M53_powered_off.png")
    messages.append(("M53 powered_off", img))
    report.append("- M53 powered_off: " + "; ".join(painter.notes))

    report.append("")
    build_sheet(messages, cols=4).save(OUT_DIR / "sheet_messages.png")
    print(f"{len(messages)} message screens")

    state0 = replace(SCREEN_0, logging="STANDARD")
    steps = pingpong_strip(state0, OUT_DIR / "screen00_ip_pingpong.png")
    print(f"screen 0 IP ping-pong: {steps} steps")
    panel, _ = render(state0, LAYOUT)
    panel.to_image(pixel_size_for_ppi(ASUS_VE278_PPI)).save(OUT_DIR / "screen00a_physical_ASUS_VE278.png")

    (OUT_DIR / "overlap_report.md").write_text("\n".join(report), encoding="utf-8")
    print(f"-> {OUT_DIR}")


if __name__ == "__main__":
    main()
