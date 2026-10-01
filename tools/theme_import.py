#!/usr/bin/env python3
"""Convert a colour theme from another terminal into a vtcon profile.

Reads the formats the theme sites hand out and prints the profile text that
goes in a [profile <name>] section of ENVARC:up-term/up-term:

    Terminal.app   foo.terminal     plist, colour components as <real> 0..1
    iTerm2         foo.itermcolors  the same plist shape
    Alacritty      foo.toml         [colors.normal] / [colors.bright]
    Warp           foo.yaml         terminal_colors: {normal:, bright:}
    Ghostty        foo              flat "palette = 0=#RRGGBB" key=value

The format is sniffed from the content, so a renamed file still works.

    tools/theme_import.py Apprentice.itermcolors
    tools/theme_import.py --name night --out night.conf theme.toml

Colour order is the xterm one everywhere: 0-7 are the normal colours, 8-15
their bright counterparts, black red green yellow blue magenta cyan white.
"""

import argparse
import os
import plistlib
import re
import sys

NORMAL = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"]
SIX = re.compile(r"\A[0-9a-fA-F]{6}\Z")
RGB = re.compile(r"\A#?([0-9a-fA-F]{3}|[0-9a-fA-F]{6})\Z")


def _hex(value):
    """'#87afd7', '87afd7', '87a', '0x87afd7' -> (r, g, b)"""
    value = value.strip().strip('"').strip("'")
    if value[:2].lower() == "0x":
        value = value[2:]
    if not RGB.match(value):
        raise ValueError("not a colour: %r" % value)
    value = value.lstrip("#")
    if len(value) == 3:
        value = "".join(c * 2 for c in value)
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def _pair(palette):
    for index in range(16):
        if index not in palette:
            raise ValueError("theme has no colour %d" % index)
    return [palette[i] for i in range(16)]


def _plist(text):
    """Terminal.app / iTerm2: <key>NAME</key> then a dict of components.

    The components arrive as <real> 0..1, or <integer> 0..65535 from older
    Terminal.app exports, and the dict may carry a Color Space and an Alpha
    Component around them, so the three are picked out by name.
    """
    def component(block, name):
        found = re.search(r"<key>%s</key>\s*<(real|integer)>([^<]+)</\1>" % name, block)
        if not found:
            return None
        value = float(found.group(2))
        if found.group(1) == "integer":       # 16-bit, the pre-10.9 exports
            value /= 65535.0
        return max(0, min(255, round(value * 255)))

    def colour(*names):
        for name in names:
            found = re.search(r"<key>\s*%s\s*</key>\s*<dict>(.*?)</dict>" % name,
                              text, re.S)
            if not found:
                continue
            block = found.group(1)
            got = tuple(component(block, c) for c in
                        ("Red Component", "Green Component", "Blue Component"))
            if all(v is not None for v in got):
                return got
        return None

    palette = {}
    for i in range(16):
        got = colour("Ansi %d Color" % i)
        if got:
            palette[i] = got
    return {
        "palette": palette,
        "bg": colour("Background Color"),
        "fg": colour("Text Color", "Foreground Color"),
        "cursor": colour("Cursor Color"),
        "selection_bg": colour("Selection Color"),
        "selection_fg": colour("Selected Text Color", "Selection Foreground Color"),
        "name": (re.search(r"<key>\s*Profile\.Name\s*</key>\s*<string>([^<]*)</string>", text)
                 or re.search(r"<key>\s*name\s*</key>\s*<string>([^<]*)</string>", text)),
    }


def _nscolour(value):
    """An NSColor out of a Terminal.app profile: an archived binary plist."""
    if isinstance(value, (bytes, bytearray)):
        try:
            value = plistlib.loads(bytes(value))
        except Exception:
            return None
    if not isinstance(value, dict):
        return None
    # $objects[1] is the NSColor itself
    if "NSRGB" in value:
        raw = value["NSRGB"]
        raw = raw.decode("ascii", "replace") if isinstance(raw, bytes) else raw
        parts = raw.replace("\0", " ").split()
        if len(parts) >= 3:
            got = tuple(max(0, min(255, round(float(p) * 255))) for p in parts[:3])
            return got if all(isinstance(v, int) for v in got) else None
    objects = value.get("$objects")
    if isinstance(objects, list):
        for item in objects:
            if isinstance(item, dict) and item is not value:
                got = _nscolour(item)
                if got:
                    return got
    comps = [value.get(k) for k in
             ("NSRedComponent", "NSGreenComponent", "NSBlueComponent")]
    if all(isinstance(c, (int, float)) for c in comps):
        # the calibrated colour space carries 16-bit components
        scale = 65535.0 if max(comps) > 1.0 else 255.0
        return tuple(max(0, min(255, round(c / scale * 255))) for c in comps)
    return None


def _terminal(text):
    """Terminal.app's .terminal profile: a plist of archived NSColours."""
    data = plistlib.loads(text.encode("latin-1") if isinstance(text, str) else text)
    if not isinstance(data, dict):
        raise ValueError("not a Terminal.app profile")

    def take(*names):
        for name in names:
            if name in data:
                got = _nscolour(data[name])
                if got:
                    return got
        return None

    palette = {}
    for i, name in enumerate(NORMAL):
        got = take("ANSI%sColor" % name.capitalize(), "ANSI%sColor" % name)
        if got:
            palette[i] = got
        bright = take("ANSIBright%sColor" % name.capitalize(), "ANSIBright%sColor" % name)
        if bright:
            palette[8 + i] = bright
    name = data.get("name")
    return {
        "palette": palette,
        "bg": take("BackgroundColor", "Background Color"),
        "fg": take("TextColor", "Text Color"),
        "cursor": take("CursorColor", "Cursor Color"),
        "selection_bg": take("SelectionColor", "Selection Color"),
        "selection_fg": None,
        "name": name if isinstance(name, str) else None,
    }


def _keyvalue(text):
    """Ghostty's flat form: 'palette = 0=#1c1c1c', 'background = #262626'."""
    palette = {}
    for index, value in re.findall(
            r"^palette\s*=\s*(\d+)\s*=\s*(#[0-9a-fA-F]{3,6})\s*$", text, re.M):
        palette[int(index)] = _hex(value)
    simple = dict((k.lower(), v) for k, v in re.findall(
        r"^(background|foreground|cursor-color|cursor-text|selection-background|"
        r"selection-foreground)\s*=\s*(#[0-9a-fA-F]{3,6})\s*$", text, re.M))
    return {
        "palette": palette,
        "bg": _hex(simple["background"]) if "background" in simple else None,
        "fg": _hex(simple["foreground"]) if "foreground" in simple else None,
        "cursor": _hex(simple["cursor-color"]) if "cursor-color" in simple else None,
        "selection_bg": (_hex(simple["selection-background"])
                         if "selection-background" in simple else None),
        "selection_fg": (_hex(simple["cursor-text"])
                         if "cursor-text" in simple else None),
        "name": re.search(r"^name\s*=\s*(.+)$", text, re.M),
    }


def _toml(text):
    """Alacritty: [colors.primary] / [colors.normal] / [colors.bright]."""
    palette, section = {}, None
    # primary, cursor and selection all carry keys called background/text, so
    # they get their own slots -- one shared dict let [colors.selection]
    # background overwrite [colors.primary] background
    primary, cursor, selection = {}, {}, {}
    slots = {"primary": (primary, ("foreground", "background")),
             "cursor": (cursor, ("text", "cursor")),
             "selection": (selection, ("background", "text"))}
    for line in text.splitlines():
        head = re.match(r"\s*\[colors\.(\w+)\]\s*$", line)
        if head:
            section = head.group(1)
            continue
        if re.match(r"\s*\[", line):
            section = None
            continue
        pair = re.match(r'\s*(\w+)\s*=\s*"?\s*(#[0-9a-fA-F]{3,6})\s*"?', line)
        if not pair or not section:
            continue
        key, value = pair.group(1), _hex(pair.group(2))
        if section in ("normal", "bright"):
            if key in NORMAL:
                palette[(0 if section == "normal" else 8) + NORMAL.index(key)] = value
        elif section in slots and key in slots[section][1]:
            slots[section][0][key] = value
    return {
        "palette": palette,
        "bg": primary.get("background"),
        "fg": primary.get("foreground"),
        "cursor": cursor.get("cursor"),
        "selection_bg": selection.get("background"),
        "selection_fg": selection.get("text"),
        "name": re.search(r'^\s*name\s*=\s*"?([^"\n]+)"?', text, re.M),
    }


def _yaml(text):
    """Warp/Ghostty YAML: terminal_colors: {normal: {...}, bright: {...}}."""
    palette = {}
    for section, offset in (("normal", 0), ("bright", 8)):
        block = re.search(r"^\s*%s:\s*$(.*?)(?=^\s*(?:bright|normal|cursor|selection)"
                          r"\s*:|^\w|\Z)" % section, text, re.S | re.M)
        if not block:
            continue
        for key, value in re.findall(r"^\s*(\w+):\s*\"?(#[0-9a-fA-F]{3,6})\"?",
                                     block.group(1), re.M):
            if key in NORMAL:
                palette[offset + NORMAL.index(key)] = _hex(value)
    flat = dict((k.lower(), v) for k, v in re.findall(
        r"^(background|foreground|cursor|selection-background|selection-foreground)"
        r":\s*[\"']?(#[0-9a-fA-F]{3,6})[\"']?", text, re.M))
    return {
        "palette": palette,
        "bg": _hex(flat["background"]) if "background" in flat else None,
        "fg": _hex(flat["foreground"]) if "foreground" in flat else None,
        "cursor": _hex(flat["cursor"]) if "cursor" in flat else None,
        "selection_bg": (_hex(flat["selection-background"])
                         if "selection-background" in flat else None),
        "selection_fg": (_hex(flat["selection-foreground"])
                         if "selection-foreground" in flat else None),
        "name": re.search(r'^\s*name:\s*"?([^"\n]+)"?', text, re.M),
    }


def sniff(text):
    """Pick the parser from what the file actually contains."""
    if "<key>Ansi 0 Color</key>" in text or "<key>Background Color</key>" in text:
        return _plist
    if "<key>ANSIBlackColor</key>" in text or "$archiver" in text:
        return _terminal
    if re.search(r"^\s*\[colors\.", text, re.M):
        return _toml
    if re.search(r"^palette\s*=", text, re.M):
        return _keyvalue
    if re.search(r"^\s*(?:terminal_colors|bright|normal|background):", text, re.M):
        return _yaml
    if re.search(r"^\w[\w -]*\s*=\s*", text, re.M):
        return _keyvalue
    raise ValueError("not a recognised terminal theme")


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    theme = sniff(text)(text)
    theme["palette"] = _pair(theme["palette"])
    for key in ("bg", "fg", "cursor", "selection_bg", "selection_fg"):
        if theme.get(key) is not None and not isinstance(theme[key], tuple):
            theme[key] = None
    name = theme.get("name")
    if hasattr(name, "group"):        # the regex parsers hand back a match
        name = name.group(1)
    theme["name"] = name.strip() if isinstance(name, str) and name.strip() else None
    if not theme["name"]:
        # the flat and TOML formats carry no name, so a folder of downloads
        # would land every theme as "imported" -- take the file's own name
        theme["name"] = re.sub(r"\.(itermcolors|terminal|toml|ya?ml|conf)$", "",
                               os.path.basename(path)).replace("_", " ").replace("-", " ")
    return theme


def profile(theme, name=None):
    """The [profile <name>] text for vtcon."""
    def word(rgb):
        return "".join("%02X" % v for v in rgb)

    lines = ["[profile %s]" % (name or theme["name"] or "imported")]
    if theme["fg"]:
        lines.append("fg = %s" % word(theme["fg"]))
    if theme["bg"]:
        lines.append("bg = %s" % word(theme["bg"]))
    if theme["cursor"]:
        lines.append("cursor-color = %s" % word(theme["cursor"]))
    if theme["selection_bg"]:
        lines.append("selection-bg = %s" % word(theme["selection_bg"]))
    if theme["selection_fg"]:
        lines.append("selection-fg = %s" % word(theme["selection_fg"]))
    if theme["palette"]:
        lines.append("palette = " + ",".join(
            "%d,%s" % (i, word(rgb)) for i, rgb in enumerate(theme["palette"])))
    return "\n".join(lines) + "\n"


def _selftest():
    ok = 0
    bad = []

    def check(what, got, want):
        nonlocal ok
        if got == want:
            ok += 1
        else:
            bad.append("%s: got %r, want %r" % (what, got, want))

    check("hex long", _hex("#87afd7"), (0x87, 0xAF, 0xD7))
    check("hex bare", _hex("87afd7"), (0x87, 0xAF, 0xD7))
    check("hex short", _hex("87a"), (0x88, 0x77, 0xAA))
    check("hex 0x", _hex("0x87afd7"), (0x87, 0xAF, 0xD7))

    # every format must yield the same 16 colours for the same theme
    expect = [(0x1C, 0x1C, 0x1C), (0xAF, 0x5F, 0x5F), (0x5F, 0x87, 0x5F),
              (0x87, 0x87, 0x5F), (0x5F, 0x87, 0xAF), (0x5F, 0x5F, 0x87),
              (0x5F, 0x87, 0x87), (0x6C, 0x6C, 0x6C), (0x44, 0x44, 0x44),
              (0xFF, 0x87, 0x00), (0x87, 0xAF, 0x87), (0xFF, 0xFF, 0xAF),
              (0x87, 0xAF, 0xD7), (0x87, 0x87, 0xAF), (0x5F, 0xAF, 0xAF),
              (0xFF, 0xFF, 0xFF)]
    expect_b, expect_f = (0x26, 0x26, 0x26), (0xBC, 0xBC, 0xBC)

    keyvalue = "[colors]\nbackground = #262626\nforeground = #bcbcbc\n" + "".join(
        "palette = %d=#%02x%02x%02x\n" % (i, *rgb) for i, rgb in enumerate(expect))
    toml = "[colors.primary]\nbackground = \"#262626\"\nforeground = \"#bcbcbc\"\n" \
           "[colors.cursor]\ntext = \"#262626\"\ncursor = \"#bcbcbc\"\n" \
           "[colors.selection]\ntext = \"#262626\"\nbackground = \"#87afd7\"\n" \
           "[colors.normal]\n" + "".join(
               '%s = "#%02x%02x%02x"\n' % (NORMAL[i % 8], *expect[i])
               for i in range(8)) + "[colors.bright]\n" + "".join(
               '%s = "#%02x%02x%02x"\n' % (NORMAL[i % 8], *expect[8 + i])
               for i in range(8))
    yaml = 'background: "#262626"\nforeground: "#bcbcbc"\nterminal_colors:\n' \
           + "".join("  %s:\n" % s for s in ()) + \
           "  normal:\n" + "".join("    %s: \"#%02x%02x%02x\"\n" % (NORMAL[i], *expect[i])
                                   for i in range(8)) + \
           "  bright:\n" + "".join("    %s: \"#%02x%02x%02x\"\n" % (NORMAL[i], *expect[8 + i])
                                   for i in range(8))
    plist = "<plist><dict>\n" + "".join(
        "<key>Ansi %d Color</key><dict><key>Color Space</key><string>sRGB</string>"
        "<key>Red Component</key><real>%f</real><key>Green Component</key><real>%f</real>"
        "<key>Blue Component</key><real>%f</real></dict>\n"
        % (i, rgb[0] / 255, rgb[1] / 255, rgb[2] / 255)
        for i, rgb in enumerate(expect)) + \
        "<key>Background Color</key><dict><key>Red Component</key><real>0.149</real>" \
        "<key>Green Component</key><real>0.149</real>" \
        "<key>Blue Component</key><real>0.149</real></dict>\n" \
        "<key>Text Color</key><dict><key>Red Component</key><real>0.737</real>" \
        "<key>Green Component</key><real>0.737</real>" \
        "<key>Blue Component</key><real>0.737</real></dict>\n</dict></plist>"

    for label, body in (("keyvalue", keyvalue), ("toml", toml),
                        ("yaml", yaml), ("plist", plist)):
        got = sniff(body)(body)
        check("%s palette" % label, [got["palette"][i] for i in range(16)], expect)
        check("%s bg" % label, got["bg"], (0x26, 0x26, 0x26))
        check("%s fg" % label, got["fg"], (0xBC, 0xBC, 0xBC))
        if label == "toml":       # the one format that carries all three
            check("toml cursor", got["cursor"], (0xBC, 0xBC, 0xBC))
            check("toml cursor text is not the cursor", got["cursor"] != (0x26, 0x26, 0x26), True)
            check("toml selection bg", got["selection_bg"], (0x87, 0xAF, 0xD7))
            check("toml selection text", got["selection_fg"], (0x26, 0x26, 0x26))
            check("toml bg is not the selection bg", got["bg"], (0x26, 0x26, 0x26))

    check("plist bg rounds 0.149", sniff(plist)(plist)["bg"], (38, 38, 38))

    # Terminal.app's .terminal: a plist whose colours are archived NSColours,
    # keyed "ANSIBlackColor" rather than "Ansi 0 Color"
    def archived(rgb):
        return plistlib.dumps({"NSRGB": b"%f %f %f\x00" % tuple(c / 255 for c in rgb),
                               "NSColorSpace": 1}, fmt=plistlib.FMT_BINARY)

    terminal = {"name": "Archived", "BackgroundColor": archived(expect_b),
                "TextColor": archived(expect_f), "CursorColor": archived(expect_f)}
    for i, name in enumerate(NORMAL):
        terminal["ANSI%sColor" % name.capitalize()] = archived(expect[i])
        terminal["ANSIBright%sColor" % name.capitalize()] = archived(expect[8 + i])
    for encoded in (plistlib.FMT_XML, plistlib.FMT_BINARY):
        body = plistlib.dumps(terminal, fmt=encoded)
        if isinstance(body, bytes):
            body = body.decode("latin-1")
        got = _terminal(body)
        check("terminal palette (%s)" % ("xml" if encoded == plistlib.FMT_XML else "bin"),
              [got["palette"][i] for i in range(16)], expect)
        check("terminal bg", got["bg"], expect_b)
        check("terminal fg", got["fg"], expect_f)
        check("terminal name", got["name"], "Archived")

    # every one of the five must land on the same sixteen colours
    for label, body in (("keyvalue", keyvalue), ("toml", toml), ("yaml", yaml),
                        ("plist", plist)):
        got = sniff(body)(body)
        check("%s agrees with the rest" % label,
              [got["palette"][i] for i in range(16)], expect)

    out = profile({"palette": expect, "fg": (0xBC, 0xBC, 0xBC),
                   "bg": (0x26, 0x26, 0x26), "cursor": (0xBC, 0xBC, 0xBC),
                   "selection_bg": None, "selection_fg": None, "name": None}, "night")
    check("profile header", out.splitlines()[0], "[profile night]")
    check("profile fg", "fg = BCBCBC" in out, True)
    check("profile bg", "bg = 262626" in out, True)
    check("profile cursor", "cursor-color = BCBCBC" in out, True)
    check("profile palette", "palette = 0,1C1C1C,1,AF5F5F,2,5F875F" in out, True)
    sel = profile({"palette": expect, "fg": (0xBC, 0xBC, 0xBC), "bg": (0x26, 0x26, 0x26),
                   "cursor": (0xBC, 0xBC, 0xBC), "selection_bg": (0x87, 0xAF, 0xD7),
                   "selection_fg": (0x26, 0x26, 0x26), "name": None}, "night")
    check("profile selection bg", "selection-bg = 87AFD7" in sel, True)
    check("profile selection fg", "selection-fg = 262626" in sel, True)
    none_sel = profile({"palette": expect, "fg": None, "bg": None, "cursor": None,
                        "selection_bg": None, "selection_fg": None, "name": None}, "x")
    check("no selection keys when the theme has none",
          "selection-" in none_sel, False)
    check("profile palette is one line", len([l for l in out.splitlines()
                                              if l.startswith("palette =")]), 1)
    check("value width fits UC_MAX_VALUE",
          max(len(v) for v in re.findall(r"= ([0-9A-F,]+)$", out, re.M)) <= 159, True)

    # a theme missing a colour must be refused, not silently filled in
    broken = "[colors]\n" + "".join("palette = %d=#000000\n" % i for i in range(15))
    try:
        _pair(sniff(broken)(broken)["palette"])
        bad.append("a 15-colour theme was accepted")
    except ValueError:
        ok += 1

    for line in bad:
        print("FAIL " + line)
    print("theme_import: %d checks, %d failed" % (ok + len(bad), len(bad)))
    return 1 if bad else 0


def main(argv):
    if "--self-test" in argv:
        return _selftest()
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("themes", nargs="+", help="theme files to convert")
    parser.add_argument("--name", action="append", default=[],
                        help="profile name for the matching theme file")
    parser.add_argument("--out", help="write here instead of stdout")
    args = parser.parse_args(argv)

    chunks = []
    for i, path in enumerate(args.themes):
        theme = read(path)
        name = args.name[i] if i < len(args.name) else None
        chunks.append(profile(theme, name))
    text = "\n".join(chunks)
    if args.out:
        with open(args.out, "w", encoding="ascii") as handle:
            handle.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))