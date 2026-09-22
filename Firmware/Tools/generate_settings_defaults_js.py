# Parses Firmware/RTK_Everywhere/settings.h (and Firmware/RTK_Everywhere/RTK_Everywhere.ino,
# for a handful of #define array-size macros settings.h itself doesn't declare) and emits
# Firmware/RTK_Everywhere/AP-Config/src/settingsDefaults.js: a table of, for every Web-Config
# setting this script can resolve, (a) which platform models it is visible on, and (b) its
# firmware default value.
#
# The Web Config page uses this table to pre-populate/show-or-hide controls for the
# current platform (sent across as the "platformModel" field) BEFORE the settings CSV
# arrives, and to know what an OMITTED setting's value should be - the ESP32 only sends
# settings whose live value differs from this same default (see createSettingsString() /
# allocateEffectiveDefaultSettings() in Firmware/RTK_Everywhere/menuCommands.ino).
#
# It also emits Firmware/RTK_Everywhere/webConfigOptimizedSettings.h: the C++ side's
# skip-if-default check in createSettingsString() only omits a setting when its name is in
# that generated allowlist. That list is exactly "settings this script found a confidently-
# resolved default for" - i.e. exactly the settings the browser can correctly redisplay when
# omitted. Deriving both sides from one pass means a setting whose CSV field name doesn't
# textually match its C++ struct member (e.g. "antennaHeight" vs. "settings.antennaHeight_mm")
# safely falls back to "always sent in full" on BOTH sides, instead of the browser silently
# guessing wrong.
#
# Two categories of setting are covered:
#  - Scalar settings (one CSV field <-> one struct member <-> one DOM element).
#  - "Suffix" array settings, whose table row expands into several CSV fields at runtime
#    (NTRIP servers x4, ESP-NOW peers, WiFi networks, corrections priority). For these,
#    createSettingsString()'s skip check still operates on the WHOLE array as one unit (see
#    commandSettingSize()), so only the base row name goes into webConfigKnownDefaultNames;
#    but the browser needs a default for each individual element, so this script expands
#    them into per-element entries in SETTINGS_DEFAULTS (e.g. "ntripServerCasterHost_0").
#    Corrections priority is a further special case: its UI (main.js) is built entirely from
#    received CSV data with no static per-element markup, so instead of per-element defaults
#    this script exports CORRECTIONS_SOURCE_DEFAULT_ORDER, which main.js's initializeArrays()
#    uses to pre-seed the priority list when the ESP32 omits it (all priorities at default).
#  - Constellation checkboxes (tLgConst/tUmConst/tMosaicConst) are also expanded into
#    per-name entries (e.g. "constellation_GPS"), reading each receiver's own name table
#    from its GNSS_*.h file - their default is always "all enabled"
#    (checkGNSSArrayDefaults() in menuSupport.ino unconditionally resets them). ZED's
#    constellations (tUbxConst) are deliberately NOT covered: constellationSupported() in
#    GNSS_ZED.ino depends on the connected receiver's firmware version, discovered at
#    runtime, so there's no static default to resolve - see the type-based special case
#    in settingHasKnownWebConfigDefault() in menuCommands.ino.
#
# This is a source-of-truth generator, not part of the build: rerun it by hand whenever
# settings.h changes (same workflow as index_html_zipper.py / main_js_zipper.py).
#   cd Firmware/Tools
#   python generate_settings_defaults_js.py
#
# Written by: Claude (for SparkFun Electronics)

import ast
import json
import os
import re
import sys

DEFAULT_SETTINGS_SOURCE = "../RTK_Everywhere/settings.h"
DEFAULT_SKETCH_SOURCE = "../RTK_Everywhere/RTK_Everywhere.ino"
DEFAULT_DEST = "../RTK_Everywhere/AP-Config/src/settingsDefaults.js"
DEFAULT_DEST_HEADER = "../RTK_Everywhere/webConfigOptimizedSettings.h"

# Per-receiver constellation name tables, each in that receiver's own .h file (next to
# settings.h) rather than settings.h itself - see resolve_constellation_names().
CONSTELLATION_SOURCE_FILES = {
    "tLgConst": "GNSS_LG290P.h",
    "tUmConst": "GNSS_UM980.h",
    "tMosaicConst": "GNSS_Mosaic.h",
}

# Scalar RTK_Settings_Types whose "name" column maps 1:1 to a single Web Config DOM
# element id.
SCALAR_TYPES = {
    "_bool", "_int", "_float", "_double", "_uint8_t", "_uint16_t", "_uint32_t",
    "_uint64_t", "_int8_t", "_int16_t", "tMuxConn", "tSysState", "tPulseEdg",
    "tBtRadio", "tPerDisp", "tCoordInp", "tCharArry", "_IPString", "tGnssReceiver",
}

# Suffix/array RTK_Settings_Types this script can expand into per-element CSV ids, and
# how those ids are built (must match the formatting in createSettingsString() /
# menuCommands.ino exactly). "index" -> name + i. GNSS message-rate/constellation
# array types are deliberately NOT here: their true default is resolved per-detected-
# receiver at runtime (see checkGNSSArrayDefaults() in menuSupport.ino) in ways this
# script can't replicate, and they're inWebConfig=0 anyway (fetched on demand, not
# part of the initial dump).
INDEXED_SUFFIX_TYPES = {
    "tNSCEn", "tNSCHost", "tNSCPort", "tNSCUser", "tNSCUsrPw", "tNSMtPt", "tNSMtPtPw",
    "tEspNowPr",
}

# Settings whose compiled struct-literal default is a sentinel (e.g. 254) resolved at
# runtime by checkGNSSArrayDefaults() based on the detected GNSS receiver (and, for
# enableExtCorrRadio on LG290P, the receiver's firmware version). The browser cannot
# precompute these, so their default is forced to null here - which keeps them out of
# webConfigOptimizedSettings.h's allowlist, so the firmware always sends them in full
# (see settingHasKnownWebConfigDefault() in Firmware/RTK_Everywhere/menuCommands.ino).
ALWAYS_SENT_NAMES = {"dynamicModel", "enableExtCorrRadio"}

ALL_MODELS = ["EVK", "FacetX5", "Torch", "Postcard", "TX2", "FPM", "FPL", "FPX"]

# CSV field name -> (actual DOM element id, value transform applied to the resolved
# default) for settings whose wire/display representation isn't the plain scalar
# value - either because parseIncoming() maps them onto a DIFFERENTLY-NAMED element
# (search main.js for "id.includes(...)" branches that call `ge()` with a different id
# than the CSV field), or because the element is a <select> dropdown whose option
# values aren't just JS's default true/false/number stringification (search
# createSettingsString() in menuCommands.ino for settings resent in a second,
# dropdown-friendly format - grep "Drop downs on the AP config page expect a"). The
# name stays keyed to the ORIGINAL row name in webConfigOptimizedSettings.h (that's
# what createSettingsString() compares against); only the SETTINGS_DEFAULTS entry's
# key/value change, so applyPlatformModel()'s generic `ge(key)` lookup in main.js
# finds the right element and sets a value one of its <option>s actually has.
SPECIAL_DOM_MAPPING = {
    # parseIncoming(): "Convert incoming mm to local meters" -> ge("antennaHeightM")
    "antennaHeight": ("antennaHeightM", lambda mm: round(mm / 1000.0, 4)),
    # <select> dropdowns with option value="1"/"0", not JS's "true"/"false" - see the
    # "Drop downs on the AP config page expect a ... value" block in
    # createSettingsString(), which resends these in this format for the same reason.
    "wifiConfigOverAP": ("wifiConfigOverAP", lambda b: 1 if b else 0),
    "tcpOverWiFiStation": ("tcpOverWiFiStation", lambda b: 1 if b else 0),
    "udpOverWiFiStation": ("udpOverWiFiStation", lambda b: 1 if b else 0),
}

# platFacetFP enum -> which of the 3 client-facing Facet FP buckets (FPM = Mosaic-X5,
# FPL = LG290P, FPX = ZED-X20P, with ZED-F9P grouped into FPX as a legacy variant) the
# setting is available on. Mirrors settingAvailableOnPlatform() in menuCommands.ino.
FACET_FP_BUCKETS = {
    "NON": set(),
    "ALL": {"FPM", "FPL", "FPX"},
    "L29": {"FPL"},
    "MX5": {"FPM"},
    "ZED": {"FPX"},  # F9P + X20P
    "ZF9": {"FPX"},  # F9P only, grouped into FPX
    "ZX2": {"FPX"},  # X20P only
    "HAS": {"FPL", "FPX"},  # LG290P + X20P
    "R33": {"FPL", "FPM"},  # LG290P + Mosaic-X5 (+ UM980, not a client bucket)
}

ROW_RE = re.compile(
    r'^\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,'
    r'\s*(\d+)\s*,\s*(\w+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*"([^"]+)"'
)

FIELD_RE = re.compile(
    r'^[ \t]*[A-Za-z_][\w:<>]*[ \t]+\*?[ \t]*(\w+)(?:\[[^\]]*\])*[ \t]*=[ \t]*([^;]+);',
    re.MULTILINE,
)


def strip_comment(line):
    idx = line.find("//")
    return line if idx < 0 else line[:idx]


def strip_all_comments(text):
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return text


def build_symbol_table(text):
    """Resolve enum members, #define NAME <int>, and const <type> NAME = <int>;
    declarations into one symbol -> integer value dict, so array-size qualifiers and
    struct defaults written as named constants can be turned into real numbers."""
    values = {}
    for m in re.finditer(r"enum(?:\s+\w+)?\s*\{([^}]*)\}", text, re.S):
        body = strip_all_comments(m.group(1))
        next_value = 0
        for member in body.split(","):
            member = member.strip()
            if not member:
                continue
            if "=" in member:
                name, rhs = (p.strip() for p in member.split("=", 1))
                if re.fullmatch(r"-?\d+", rhs):
                    next_value = int(rhs)
                elif rhs in values:
                    next_value = values[rhs]
                else:
                    continue
            else:
                name = member
            values[name] = next_value
            next_value += 1

    for m in re.finditer(r"^\s*#define\s+(\w+)\s+(-?\d+)\b", text, re.M):
        values.setdefault(m.group(1), int(m.group(2)))

    for m in re.finditer(r"\bconst\s+[\w:]+\s+(\w+)\s*=\s*(-?\d+)\s*;", text):
        values.setdefault(m.group(1), int(m.group(2)))

    return values


def resolve_qualifier(token, symbols):
    token = token.strip()
    if re.fullmatch(r"-?\d+", token):
        return int(token)
    return symbols.get(token)


def eval_constant_arithmetic(expr):
    """Evaluate a simple constant integer/float arithmetic expression (e.g. the
    "24 * 60" in `autoFirmwareCheckMinutes = 24 * 60;`), restricted to +-*/ and unary
    -/+ on numeric literals only - no names, calls, or attribute access - so this is
    safe to run on arbitrary text pulled out of settings.h. Returns None if `expr`
    isn't exactly such an expression."""
    try:
        node = ast.parse(expr, mode="eval").body
    except SyntaxError:
        return None

    ALLOWED_BINOPS = (ast.Add, ast.Sub, ast.Mult, ast.Div, ast.FloorDiv, ast.Mod)

    def evaluate(n):
        if isinstance(n, ast.Constant) and isinstance(n.value, (int, float)):
            return n.value
        if isinstance(n, ast.BinOp) and isinstance(n.op, ALLOWED_BINOPS):
            left, right = evaluate(n.left), evaluate(n.right)
            if left is None or right is None:
                return None
            if isinstance(n.op, ast.Add):
                return left + right
            if isinstance(n.op, ast.Sub):
                return left - right
            if isinstance(n.op, ast.Mult):
                return left * right
            if isinstance(n.op, ast.Div):
                return left / right
            if isinstance(n.op, ast.FloorDiv):
                return left // right
            if isinstance(n.op, ast.Mod):
                return left % right
        if isinstance(n, ast.UnaryOp) and isinstance(n.op, (ast.UAdd, ast.USub)):
            operand = evaluate(n.operand)
            return None if operand is None else (operand if isinstance(n.op, ast.UAdd) else -operand)
        return None

    return evaluate(node)


def resolve_literal(raw, enum_values):
    """Turn a C initializer's raw text into a JSON-able Python value, or None if it
    can't be confidently resolved (the caller then falls back to always sending that
    setting in full - see the module docstring)."""
    raw = raw.strip()
    if raw in ("true", "false"):
        return raw == "true"
    if re.fullmatch(r"-?\d+", raw):
        return int(raw)
    if re.fullmatch(r"-?\d*\.\d+|-?\d+\.\d*", raw):
        return float(raw)
    if re.fullmatch(r'"[^"]*"', raw):
        return raw[1:-1]
    if raw in enum_values:
        return enum_values[raw]
    if re.fullmatch(r"[-+*/%.\d\s()]+", raw) and any(c.isdigit() for c in raw):
        value = eval_constant_arithmetic(raw)
        if value is not None:
            return value
    return None


def find_matching_brace(text, open_index):
    """text[open_index] must be '{'. Returns the index of the matching '}'."""
    depth = 0
    i = open_index
    in_string = False
    while i < len(text):
        c = text[i]
        if in_string:
            if c == "\\":
                i += 1
            elif c == '"':
                in_string = False
        else:
            if c == '"':
                in_string = True
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    raise ValueError("No matching brace found")


def split_top_level(text):
    """Split text on commas that sit outside nested {}/()/[]/quotes."""
    parts = []
    depth = 0
    in_string = False
    current = []
    i = 0
    while i < len(text):
        c = text[i]
        if in_string:
            current.append(c)
            if c == "\\" and i + 1 < len(text):
                i += 1
                current.append(text[i])
            elif c == '"':
                in_string = False
            i += 1
            continue
        if c == '"':
            in_string = True
            current.append(c)
        elif c in "{([":
            depth += 1
            current.append(c)
        elif c in "})]":
            depth -= 1
            current.append(c)
        elif c == "," and depth == 0:
            parts.append("".join(current))
            current = []
        else:
            current.append(c)
        i += 1
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return [p.strip() for p in parts if p.strip()]


def extract_array_block(text, field_name):
    """Find `field_name[...]... = { ... };` (possibly `[N][M]`) anywhere in text and
    return the comment-stripped text between the outer braces, or None if not found."""
    m = re.search(r"\b" + re.escape(field_name) + r"\s*(?:\[[^\]]*\])+\s*=", text)
    if not m:
        return None
    brace_start = text.index("{", m.end())
    brace_end = find_matching_brace(text, brace_start)
    return strip_all_comments(text[brace_start + 1 : brace_end])


def struct_field_name_from_var(var_text):
    """"& settings.ntripServer_CasterHost[0]" -> "ntripServer_CasterHost"."""
    m = re.search(r"settings\.(\w+)", var_text)
    return m.group(1) if m else None


def parse_struct_defaults(text, symbols):
    start = text.index("struct Settings")
    end = text.index("\n} settings;", start) + len("\n} settings;")
    struct_body = strip_all_comments(text[start:end])

    defaults = {}
    for m in FIELD_RE.finditer(struct_body):
        name, raw = m.group(1), m.group(2)
        value = resolve_literal(raw, symbols)
        if value is not None:
            defaults[name] = value
    return defaults


def parse_settings_table(text):
    start = text.index("const RTK_Settings_Entry rtkSettingsEntries[] =")
    end = text.index("\n};", start) + len("\n};")
    table_body = text[start:end]

    rows = []  # (name, type, qualifier_token, var_text, platforms:set)
    for line in table_body.splitlines():
        m = ROW_RE.match(strip_comment(line))
        if not m:
            continue
        (inWebConfig, inCommands, useSuffix, platEvk, platFacetMosaic, platTorch,
         platPostcard, platFacetFP, platTorchX2, type_, qualifier, var_text, name) = m.groups()

        if inWebConfig != "1":
            continue
        known_types = SCALAR_TYPES | INDEXED_SUFFIX_TYPES | {"tWiFiNet", "tCorrSPri"} | set(CONSTELLATION_SOURCE_FILES)
        if type_ not in known_types:
            continue

        platforms = set()
        if platEvk == "1":
            platforms.add("EVK")
        if platFacetMosaic == "1":
            platforms.add("FacetX5")
        if platTorch == "1":
            platforms.add("Torch")
        if platPostcard == "1":
            platforms.add("Postcard")
        if platTorchX2 == "1":
            platforms.add("TX2")
        platforms |= FACET_FP_BUCKETS.get(platFacetFP, set())

        rows.append((name, type_, qualifier, var_text, platforms))
    return rows


def resolve_indexed_defaults(struct_text, var_text, count, elem_kind):
    """Return a list of `count` default values for a single-dimension array field
    (bool/int/string), padded with the type's zero-value if the initializer has fewer
    explicit elements than `count` (C aggregate-init rule), or None if unresolved."""
    field_name = struct_field_name_from_var(var_text)
    if field_name is None:
        return None
    block = extract_array_block(struct_text, field_name)
    if block is None:
        return None
    elements = split_top_level(block)
    zero = {"bool": False, "int": 0, "string": ""}[elem_kind]
    values = []
    for i in range(count):
        if i < len(elements):
            v = resolve_literal(elements[i], {})
            values.append(v if v is not None else zero)
        else:
            values.append(zero)
    return values


def resolve_wifi_defaults(struct_text, var_text, count):
    """wifiNetworks[N] = { {ssid, password}, ... }; -> list of (ssid, password)."""
    field_name = struct_field_name_from_var(var_text)
    if field_name is None:
        return None
    block = extract_array_block(struct_text, field_name)
    if block is None:
        return None
    elements = split_top_level(block)
    pairs = []
    for i in range(count):
        if i < len(elements) and elements[i].startswith("{"):
            inner = elements[i][1 : elements[i].rfind("}")]
            parts = split_top_level(inner)
            ssid = resolve_literal(parts[0], {}) if len(parts) > 0 else ""
            password = resolve_literal(parts[1], {}) if len(parts) > 1 else ""
            pairs.append((ssid if ssid is not None else "", password if password is not None else ""))
        else:
            pairs.append(("", ""))
    return pairs


def resolve_corrections_source_names(text):
    block = extract_array_block(text, "correctionsSourceNames")
    if block is None:
        return None
    names = []
    for element in split_top_level(block):
        v = resolve_literal(element, {})
        if isinstance(v, str):
            names.append(v)
    return names


def struct_array_field(text, array_name, field_index):
    """Parse `array_name[] = { {a, b}, {a, b}, ... };` and return the string at
    `field_index` of each element, or None if the array can't be found. Used for the
    three receivers' constellation name tables, which are structs of {visible name,
    config command} rather than a plain string array (see GNSS_LG290P.h,
    GNSS_UM980.h, GNSS_Mosaic.h)."""
    block = extract_array_block(text, array_name)
    if block is None:
        return None
    names = []
    for element in split_top_level(block):
        if not element.startswith("{"):
            continue
        inner = element[1 : element.rfind("}")]
        parts = split_top_level(inner)
        if len(parts) > field_index:
            v = resolve_literal(parts[field_index], {})
            if isinstance(v, str):
                names.append(v)
    return names if names else None


def resolve_constellation_names(type_, text):
    """Per-receiver constellation visible-name list, in the order createSettingsString()
    (via each receiver's own _createString callback) emits them, or None if not found."""
    if type_ == "tLgConst":
        # const char *lg290pConstellationNames[] = {"GPS", "GLONASS", ...};
        block = extract_array_block(text, "lg290pConstellationNames")
        if block is None:
            return None
        names = [resolve_literal(e, {}) for e in split_top_level(block)]
        names = [n for n in names if isinstance(n, str)]
        return names if names else None
    if type_ == "tUmConst":
        # const um980ConstellationCommand um980ConstellationCommands[] = {{textName, textCommand}, ...};
        # emitted using .textName (field 0).
        return struct_array_field(text, "um980ConstellationCommands", 0)
    if type_ == "tMosaicConst":
        # const mosaicSignalConstellation mosaicSignalConstellations[] = {{name, configName}, ...};
        # emitted using .configName (field 1).
        return struct_array_field(text, "mosaicSignalConstellations", 1)
    return None


def main():
    settings_filename = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SETTINGS_SOURCE
    dest_filename = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_DEST
    dest_header_filename = sys.argv[3] if len(sys.argv) > 3 else DEFAULT_DEST_HEADER
    sketch_filename = sys.argv[4] if len(sys.argv) > 4 else DEFAULT_SKETCH_SOURCE

    print()
    print("SparkFun RTK: generate settingsDefaults.js from settings.h")
    print()

    with open(settings_filename, "r", encoding="utf-8") as f:
        settings_text = f.read()
    try:
        with open(sketch_filename, "r", encoding="utf-8") as f:
            sketch_text = f.read()
    except OSError:
        sketch_text = ""

    settings_dir = os.path.dirname(settings_filename)
    constellation_texts = {}
    for type_, basename in CONSTELLATION_SOURCE_FILES.items():
        try:
            with open(os.path.join(settings_dir, basename), "r", encoding="utf-8") as f:
                constellation_texts[type_] = f.read()
        except OSError:
            constellation_texts[type_] = ""

    symbols = build_symbol_table(settings_text + "\n" + sketch_text)
    struct_defaults = parse_struct_defaults(settings_text, symbols)
    rows = parse_settings_table(settings_text)
    corrections_source_names = resolve_corrections_source_names(settings_text)

    output = {}
    known_default_names = set()
    always_sent = []

    for name, type_, qualifier_token, var_text, platforms in rows:
        if not platforms:
            continue

        if type_ == "tCorrSPri":
            # Special-cased below via CORRECTIONS_SOURCE_DEFAULT_ORDER, not per-element
            # entries in SETTINGS_DEFAULTS - see module docstring.
            if corrections_source_names is not None:
                known_default_names.add(name)
            else:
                always_sent.append(name)
            continue

        if type_ in CONSTELLATION_SOURCE_FILES:
            # All 4 receivers' constellation rows share the row name "constellation_",
            # so - unlike every other type here - eligibility is NOT driven by
            # webConfigKnownDefaultNames (name-only lookup can't tell them apart);
            # settingHasKnownWebConfigDefault() in menuCommands.ino special-cases this
            # by RTK_Settings_Types instead. Nothing is added to known_default_names
            # here; only the per-name SETTINGS_DEFAULTS entries matter.
            names = resolve_constellation_names(type_, constellation_texts.get(type_, ""))
            if names is None:
                always_sent.append(f"{name}(*) [{type_}]")
                continue
            for constellation_name in names:
                # Constellation names overlap across receivers (GPS, GLONASS, ...) -
                # merge platform lists rather than letting the last receiver processed
                # clobber the entry.
                entry = output.setdefault(f"{name}{constellation_name}", {"default": True, "platforms": []})
                entry["platforms"] = sorted(set(entry["platforms"]) | platforms)
            continue

        if type_ in SCALAR_TYPES:
            # Most settings' DOM element id is just their CSV name - but a handful are
            # remapped by a special case in parseIncoming() (see SPECIAL_DOM_MAPPING).
            # The allowlist stays keyed by the original row `name` (that's what
            # createSettingsString() compares against); only the SETTINGS_DEFAULTS
            # entry moves to the real DOM id, with its default value converted to match.
            dom_key, transform = SPECIAL_DOM_MAPPING.get(name, (name, None))
            entry = output.setdefault(dom_key, {"default": None, "platforms": []})
            entry["platforms"] = sorted(set(entry["platforms"]) | platforms)

            if name in ALWAYS_SENT_NAMES:
                always_sent.append(name)
                continue

            field_name = struct_field_name_from_var(var_text) or name
            default = struct_defaults.get(field_name)
            if default is None:
                default = struct_defaults.get(name)
            if default is not None:
                entry["default"] = transform(default) if transform else default
                known_default_names.add(name)
            else:
                always_sent.append(name)
            continue

        # Suffix/array types: base row name goes in the allowlist (the C++ skip check
        # compares the WHOLE array to its default in one memcmp), individual elements
        # get per-index entries in SETTINGS_DEFAULTS so the browser knows what each one
        # should show when the whole array is omitted.
        count = resolve_qualifier(qualifier_token, symbols)
        if count is None:
            always_sent.append(name)
            continue

        if type_ == "tWiFiNet":
            pairs = resolve_wifi_defaults(settings_text, var_text, count)
            if pairs is None:
                always_sent.append(name)
                continue
            known_default_names.add(name)
            for i, (ssid, password) in enumerate(pairs):
                output[f"{name}{i}SSID"] = {"default": ssid, "platforms": sorted(platforms)}
                output[f"{name}{i}Password"] = {"default": password, "platforms": sorted(platforms)}
            continue

        if type_ in INDEXED_SUFFIX_TYPES:
            elem_kind = {
                "tNSCEn": "bool", "tNSCHost": "string", "tNSCPort": "int",
                "tNSCUser": "string", "tNSCUsrPw": "string", "tNSMtPt": "string",
                "tNSMtPtPw": "string", "tEspNowPr": "string",
            }[type_]
            if type_ == "tEspNowPr":
                # espnowPeers[N][6] = {0}; - a zeroed MAC address for every peer.
                values = ["00:00:00:00:00:00"] * count
            else:
                values = resolve_indexed_defaults(settings_text, var_text, count, elem_kind)
            if values is None:
                always_sent.append(name)
                continue
            known_default_names.add(name)
            for i, v in enumerate(values):
                output[f"{name}{i}"] = {"default": v, "platforms": sorted(platforms)}
            continue

    # baseTypeSurveyIn/baseTypeFixed/fixedBaseCoordinateTypeECEF/...Geo aren't
    # settings.h table rows at all - settings.fixedBase and
    # settings.fixedBaseCoordinateType are (inWebConfig=0), and createSettingsString()
    # derives these four radio-button-friendly booleans from them by hand. Derive
    # their defaults the same way here, and allowlist the two underlying struct
    # fields so createSettingsString() can skip the block that emits them.
    fixed_base_default = struct_defaults.get("fixedBase")
    if fixed_base_default is not None:
        output["baseTypeSurveyIn"] = {"default": not fixed_base_default, "platforms": ALL_MODELS}
        output["baseTypeFixed"] = {"default": bool(fixed_base_default), "platforms": ALL_MODELS}
        known_default_names.add("fixedBase")
    else:
        always_sent.append("fixedBase")

    fixed_coord_type_default = struct_defaults.get("fixedBaseCoordinateType")
    if fixed_coord_type_default is not None:
        output["fixedBaseCoordinateTypeECEF"] = {"default": not bool(fixed_coord_type_default), "platforms": ALL_MODELS}
        output["fixedBaseCoordinateTypeGeo"] = {"default": bool(fixed_coord_type_default), "platforms": ALL_MODELS}
        known_default_names.add("fixedBaseCoordinateType")
    else:
        always_sent.append("fixedBaseCoordinateType")

    if always_sent:
        print("These Web Config settings have no resolvable default, so the firmware will "
              "always send them in full (safe, just no byte savings for them):")
        for name in sorted(set(always_sent)):
            print(f"  - {name}")
    print()

    with open(dest_filename, "w", encoding="utf-8", newline="\n") as f:
        f.write("// AUTO-GENERATED by Firmware/Tools/generate_settings_defaults_js.py from settings.h.\n")
        f.write("// Do not hand-edit - rerun the script instead.\n")
        f.write("const SETTINGS_DEFAULTS = ")
        f.write(json.dumps(output, indent=2, sort_keys=True))
        f.write(";\n\n")
        f.write("// Default corrections-source priority order (identity order - see\n")
        f.write("// allocateEffectiveDefaultSettings() in menuCommands.ino). Used by\n")
        f.write("// initializeArrays() in main.js to pre-seed the priority list so it's still\n")
        f.write("// correct even if the ESP32 omits every correctionsPriority_* entry.\n")
        f.write("const CORRECTIONS_SOURCE_DEFAULT_ORDER = ")
        f.write(json.dumps(corrections_source_names or []))
        f.write(";\n")

    with open(dest_header_filename, "w", encoding="utf-8", newline="\n") as f:
        f.write("// AUTO-GENERATED by Firmware/Tools/generate_settings_defaults_js.py from settings.h.\n")
        f.write("// Do not hand-edit - rerun the script instead.\n")
        f.write("// See settingHasKnownWebConfigDefault() in menuCommands.ino for how this is used.\n")
        f.write("static const char *const webConfigKnownDefaultNames[] = {\n")
        for name in sorted(known_default_names):
            f.write(f'    "{name}",\n')
        f.write("};\n")
        f.write("static const int webConfigKnownDefaultNamesCount =\n"
                 "    sizeof(webConfigKnownDefaultNames) / sizeof(webConfigKnownDefaultNames[0]);\n")

    print(f"Wrote {len(output)} SETTINGS_DEFAULTS entries and {len(known_default_names)} "
          f"webConfigKnownDefaultNames to:")
    print(f"  {dest_filename}")
    print(f"  {dest_header_filename}")
    print("Done!")


if __name__ == "__main__":
    main()
