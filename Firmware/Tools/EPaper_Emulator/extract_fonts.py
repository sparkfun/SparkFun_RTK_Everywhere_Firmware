######################################################################################
#
# extract_fonts.py
#
# Parses the e-paper fonts (res/_fnt_*.h) out of the SparkFun SSD168x I2C Interface
# Library and dumps them to fonts_ep.json, so the emulator draws the exact same
# pixelated glyphs as the firmware instead of a TrueType approximation.
#
# Use: python extract_fonts.py [path/to/SparkFun_SSD168x_I2C_Interface_Library]
#
######################################################################################

import json
import re
import sys
from pathlib import Path

DEFAULT_LIBRARY = Path.home() / "Dropbox/Apps/Arduino/user/libraries/SparkFun_SSD168x_I2C_Interface_Library"
OUT_JSON = Path(__file__).parent / "fonts_ep.json"

# QW_EP_FONT_* name -> library resource file
FONT_FILES = {
    "5X7": "_fnt_5x7.h",
    "8X16": "_fnt_8x16.h",
    "10X20": "_fnt_10x20.h",
    "LARGENUM": "_fnt_largenum.h",
    "31X48": "_fnt_31x48.h",
}


def parse_font(path):
    text = path.read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    attrs = {}
    for key in ("WIDTH", "HEIGHT", "START", "NCHAR", "MAP_WIDTH"):
        m = re.search(r"#define\s+FONT_\w+?_" + key + r"\s+(\d+)", text, flags=re.IGNORECASE)
        attrs[key.lower()] = int(m.group(1))
    # The #if/#elif branches repeat the declaration line but share one initializer
    # body, so take everything from the first declaration to the closing "};"
    start = re.search(r"_data\s*\[\]", text).end()
    body = text[text.index("{", start) : text.index("};", start)]
    # (?<!\w) so "font10x20_data" on the repeated declaration lines isn't read as 0x20
    attrs["data"] = [int(b, 16) for b in re.findall(r"(?<!\w)0x[0-9A-Fa-f]{1,2}\b", body)]
    expected = attrs["map_width"] * ((attrs["height"] + 7) // 8) * -(-attrs["nchar"] // (attrs["map_width"] // attrs["width"]))
    if len(attrs["data"]) < expected:
        raise ValueError(f"{path.name}: {len(attrs['data'])} bytes, expected at least {expected}")
    return attrs


def main():
    library = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_LIBRARY
    res = library / "src" / "res"
    fonts = {name: parse_font(res / file) for name, file in FONT_FILES.items()}
    OUT_JSON.write_text(json.dumps(fonts), encoding="utf-8")
    for name, f in fonts.items():
        print(f"{name:8s} {f['width']}x{f['height']} start={f['start']} n={f['nchar']} bytes={len(f['data'])}")
    print(f"-> {OUT_JSON}")


if __name__ == "__main__":
    main()
