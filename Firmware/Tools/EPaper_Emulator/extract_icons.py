######################################################################################
#
# extract_icons.py
#
# Parses ../../RTK_Everywhere/icons.h and dumps everything the 184x88 emulator needs
# to icons_184x88.json, so nothing is hand-transcribed from the firmware:
#
#   "icons"      - every bitmap (column-major, bit 0 = top row) with width/height
#   "props"      - the DISPLAY_184x88 entry of every iconProperty / iconProperties /
#                  iconClockProperties / iconLoggingProperties / iconBatteryProperties,
#                  i.e. {bitmap, width, height, x, y} (a list for multi-state tables)
#   "correction" - correctionIconAttributes[] in CORRECTION_ID_T order
#   "broadcast"  - broadcastIconAttributes[] in BCAST_ID_T order
#   "scalars"    - the *184x88 position constants (e.g. AccuracyIconXPos184x88)
#
# Use: python extract_icons.py
#
######################################################################################

import json
import re
from pathlib import Path

ICONS_H = Path(__file__).parent.parent.parent / "RTK_Everywhere" / "icons.h"
OUT_JSON = Path(__file__).parent / "icons_184x88.json"

# settings.h: DisplayWidth[] / DisplayHeight[] for DISPLAY_64x48, DISPLAY_128x64, DISPLAY_184x88
DISPLAY_WIDTH = [64, 128, 184]
DISPLAY_HEIGHT = [48, 64, 88]
DISPLAY_184x88 = 2


def evaluate(expr, scalars):
    """Evaluate a C constant expression such as '(34 + 14 - 6)' or
    'DisplayWidth[2] - Logging_Width' using previously parsed constants."""
    expr = expr.strip()
    if not re.fullmatch(r"[\w\s\[\]()+\-*/]+", expr):
        return None
    namespace = dict(scalars, DisplayWidth=DISPLAY_WIDTH, DisplayHeight=DISPLAY_HEIGHT)
    try:
        return int(eval(expr, {"__builtins__": {}}, namespace))
    except Exception:
        return None


def main():
    text = ICONS_H.read_text(encoding="utf-8")

    # Strip comments (the ASCII-art diagrams) so they don't confuse parsing
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)

    # Scalar constants, in file order so later expressions can use earlier results
    scalars = {}
    for m in re.finditer(r"const\s+(?:int|uint8_t)\s+(\w+)\s*=\s*([^;{]+);", text):
        value = evaluate(m.group(2), scalars)
        if value is not None:
            scalars[m.group(1)] = value

    # const uint8_t <Name>[] = { 0x.., ... };  (bitmap data)
    arrays = {}
    for m in re.finditer(r"const\s+uint8_t\s+(\w+)\[\]\s*=\s*\{([^}]*)\}\s*;", text, flags=re.DOTALL):
        arrays[m.group(1)] = [int(b, 16) for b in re.findall(r"0x[0-9A-Fa-f]{1,2}", m.group(2))]

    def resolve(symbol):
        return int(symbol) if symbol.isdigit() else scalars.get(symbol)

    # Bitmap dimensions: <Name>_Width / <Name>_Height ...
    dims = {}
    for name in arrays:
        w, h = scalars.get(f"{name}_Width"), scalars.get(f"{name}_Height")
        if w is not None and h is not None:
            dims[name] = (w, h)

    # ... or from a usage site: "{ &Name, W, H, X, Y }"
    tuple_re = re.compile(r"\{\s*&(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*([^,{}]+?)\s*,\s*([^,{}]+?)\s*\}")
    for m in tuple_re.finditer(text):
        name, w, h = m.group(1), resolve(m.group(2)), resolve(m.group(3))
        if name in arrays and name not in dims and w is not None and h is not None:
            dims[name] = (w, h)

    # ... or from the correction/broadcast attribute tables: "{xOff, yOff, W, H, Name}"
    attr_re = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\}")
    attribute_tables = {}
    for table in ("correctionIconAttributes", "broadcastIconAttributes"):
        m = re.search(table + r"\[\w+\]\s*=\s*\{(.*?)\};", text, flags=re.DOTALL)
        rows = []
        for a in attr_re.finditer(m.group(1)):
            x_off, y_off, w, h, name = int(a.group(1)), int(a.group(2)), resolve(a.group(3)), resolve(a.group(4)), a.group(5)
            dims.setdefault(name, (w, h))
            rows.append({"bitmap": name, "xOffset": x_off, "yOffset": y_off, "width": w, "height": h})
        attribute_tables[table] = rows

    icons = {}
    for name, data in arrays.items():
        if name not in dims:
            continue
        w, h = dims[name]
        if len(data) != w * ((h + 7) // 8):
            continue  # dims don't match this array - skip rather than guess
        icons[name] = {"width": w, "height": h, "data": data}

    def prop_from_tuple(m):
        return {
            "bitmap": m.group(1),
            "width": resolve(m.group(2)),
            "height": resolve(m.group(3)),
            "x": evaluate(m.group(4), scalars),
            "y": evaluate(m.group(5), scalars),
        }

    # Single iconProperty structs, e.g. BTSymbol184x88
    props = {}
    for m in re.finditer(r"const\s+iconProperty\s+(\w+)\s*=\s*(\{[^;]*\})\s*;", text):
        t = tuple_re.search(m.group(2))
        if t:
            props[m.group(1)] = prop_from_tuple(t)

    # Multi-display tables: one tuple per display type (x3), possibly x N states
    for m in re.finditer(
        r"const\s+(iconProperties|iconClockProperties|iconLoggingProperties|iconBatteryProperties)\s+(\w+)\s*=\s*(\{.*?\})\s*;",
        text,
        flags=re.DOTALL,
    ):
        kind, name, body = m.groups()
        tuples = list(tuple_re.finditer(body))
        entries = [prop_from_tuple(t) for i, t in enumerate(tuples) if i % 3 == DISPLAY_184x88]
        props[name] = entries[0] if kind == "iconProperties" else entries

    out = {
        "icons": icons,
        "props": props,
        "correction": attribute_tables["correctionIconAttributes"],
        "broadcast": attribute_tables["broadcastIconAttributes"],
        "scalars": {k: v for k, v in scalars.items() if k.endswith("184x88")},
    }
    OUT_JSON.write_text(json.dumps(out, indent=1), encoding="utf-8")
    print(f"Extracted {len(icons)} icons, {len(props)} property tables -> {OUT_JSON}")
    skipped = sorted(set(arrays) - set(icons))
    if skipped:
        print(f"Skipped {len(skipped)} arrays with no resolvable Width/Height: {skipped}")


if __name__ == "__main__":
    main()
