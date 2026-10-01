#!/usr/bin/env python3
"""prefs_rig.py -- the profile config layer, on the rig (tools/rig/rig.py must be up).

Writes /ENV/up-term/up-term with two profiles, then checks, by screenshot,
that a window's look comes from the file:
  1. no PROFILE option: the built-in defaults stand (light achromatic text on
     black) -- the "no profile changes nothing" invariant;
  2. PROFILE a: the profile's fg (yellow) and bg (black);
  3. PROFILE a's palette remap: SGR 32 (ANSI green) draws in the remapped red;
  4. PROFILE b: fg cyan on bg red, and the profile's font: the row pitch is
     ~2x profile a's.
Colour checks are family tests (r/g/b relations), not exact RGB: on an 8-bit
screen ObtainBestPen lands on the nearest palette entry. Prints [PASS]/[FAIL]
per check; exits 1 on any failure. Deletes the file and test files after.

With --ui it drives the Prefs app instead (see ui_main below): the window's
Load -> edit -> Save round trip, checked by reading the file back.

Not covered here (needs a wheel mouse / a human's eye): the mouse-wheel
direction (IntuiWheelData delta sign; one line in handler/vtcon_handler.c if
backwards), the visual-bell flash, copy-on-select's clipboard content.
"""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
from colour_check import pixels

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = []
fails = []

def check(name, ok, detail=""):
    print("[%s] %s %s" % ("PASS" if ok else "FAIL", name, detail))
    if not ok:
        fails.append(name)

def put(path, data):
    tmp = os.path.join(HERE, "_prefsrig_tmp")
    open(tmp, 'wb').write(data)
    ami.main(['put', tmp, path])
    os.unlink(tmp)

def fam(p):
    """a colour family: b=black, y=yellow, r=red, c=cyan, g=grey-ish light"""
    r, g, b = p
    if r < 60 and g < 60 and b < 60:
        return 'b'
    if abs(r - g) < 30 and abs(g - b) < 30 and r > 150:
        return 'g'
    if r > 150 and g < 120 and b < 120:
        return 'r'
    if r > 150 and g > 150 and b < 120:
        return 'y'
    if g > 150 and b > 150 and r < 120:
        return 'c'
    return '?'

def ink_at(px, x, y, bg):
    """the cell's ink: the pixel in its middle area furthest from the
    window's background, so a glyph stroke is picked, not the background"""
    best, bd = px(x, y), -1
    for dy in range(-4, 5):
        for dx in range(-4, 5):
            p = px(x + dx, y + dy)
            d = sum((a - b) ** 2 for a, b in zip(p, bg)) ** 0.5
            if d > bd:
                best, bd = p, d
    return best

def scan_ink_row(px, x, y0, y1, want, bg):
    """the first line at/below y0 whose cell at x shows ink of family want"""
    for y in range(y0, y1, 2):
        if fam(ink_at(px, x, y, bg)) == want:
            return y
    return None

def open_win(title, spec):
    ami.req(0x02, struct.pack('>H', 30) +
            ('run >NIL: newshell "XCON:0/12/640/300/%s/CLOSE"' % spec).encode('latin-1'))
    time.sleep(4)
    for _ in range(3):
        w = ami.window(title)
        if w:
            return w
        time.sleep(2)
    raise SystemExit("window %r never appeared" % title)

def shot(out):
    ami.main(['shot', out]); SHOTS.append(out)

def main():
    # two profiles, and no "default" section: a window without PROFILE <name>
    # falls back to the built-in look (light text on black).
    put('ENV:up-term/up-term',
        b'[profile a]\nfg = FFFF00\nbg = 000000\npalette = 2,FF0000\n'
        b'[profile b]\nfg = 00FFFF\nbg = FF0000\nfont = TOPAZ:16\n')
    # the bytes to Type: an ANSI-green word and a plain word, one per line
    put('VTC:prefsa.txt', b'\x1b[32mGGGG\x1b[0m\nHHHH\n')
    put('VTC:prefsb.txt', b'WWWW\nWWWW\n')
    out = os.path.join(HERE, "../../build/rig/shots/prefs_rig.png")
    x = 20   # inside the window's first column area

    # 1: no profile -> the built-in defaults: black bg, light achromatic text
    open_win('prefs0', 'CLOSE')
    ami.main(['type', 'Type VTC:prefsb.txt'])
    time.sleep(2)
    shot(out); _, _, px = pixels(out)
    bg = fam(px(x, 100))
    check("no-profile background is black", bg == 'b', "fam=%s" % bg)
    ty = scan_ink_row(px, x, 30, 200, 'g', (0, 0, 0))
    check("no-profile text is light (built-in grey)", ty is not None, "y=%s" % ty)

    # 2 + 3: profile a: yellow on black, ANSI green remapped to red
    open_win('prefsA', 'PROFILE a')
    ami.main(['type', 'Type VTC:prefsa.txt'])
    time.sleep(2)
    shot(out); _, _, px = pixels(out)
    check("profile a background is black", fam(px(x, 100)) == 'b')
    yg = scan_ink_row(px, x, 30, 200, 'r', (0, 0, 0))   # GGGG: remapped green
    yh = scan_ink_row(px, x, (yg or 30) + 4, 220, 'y', (0, 0, 0))  # HHHH
    check("profile a palette: SGR 32 draws red", yg is not None, "y=%s" % yg)
    check("profile a foreground is yellow", yh is not None, "y=%s" % yh)
    step_a = (yh - yg) if (yg and yh) else None

    # 4: profile b: cyan on red, topaz 16 -> the row pitch doubles
    open_win('prefsB', 'PROFILE b')
    ami.main(['type', 'Type VTC:prefsb.txt'])
    time.sleep(2)
    shot(out); _, _, px = pixels(out)
    check("profile b background is red", fam(px(x, 100)) == 'r')
    w1 = scan_ink_row(px, x, 30, 220, 'c', (255, 0, 0))
    w2 = scan_ink_row(px, x, (w1 or 30) + 4, 240, 'c', (255, 0, 0))
    check("profile b foreground is cyan", w1 is not None, "y=%s" % w1)
    step_b = (w2 - w1) if (w1 and w2) else None
    check("profile b font: row pitch ~2x profile a",
          step_a and step_b and 1.5 <= step_b / step_a <= 2.5,
          "pitch a=%s b=%s" % (step_a, step_b))

    # cleanup: the rig goes back to no config (the built-in look)
    for p in ('ENV:up-term/up-term', 'VTC:prefsa.txt', 'VTC:prefsb.txt'):
        try:
            ami.main(['exec', 'Delete %s QUIET' % p])
        except SystemExit:
            pass
    report()
    print("all prefs checks passed; shot in build/rig/shots/prefs_rig.png")

def report():
    if fails:
        print("FAILURES: %s" % ", ".join(fails))
        print("shots: %s" % " ".join(os.path.relpath(s) for s in SHOTS))
        sys.exit(1)

# ---- the Prefs app on the rig (--ui): the round trip through the window ----
#
# The app's contract is the file: what the window shows is what Save writes.
# So drive the window (Load, edit the font field, Save, close) and read the
# file back. The app is put on the rig as VTC:upprefs (the kit installs it as
# "C:UP-Term Prefs"; a space in the rig's run command would break the CLI, the
# name itself is Install's business, not the app's).

BACKSPACE = 0x48
WIN = 'UP-Term Prefs'

def click(box, right=False):
    """a click in a uitree gadget box: at its right end when right (so the
    caret lands after the text of a string gadget), else in the middle"""
    x, y, w, h = box
    cx = x + (w - 4 if right else w // 2)
    cy = y + h // 2
    kx, ky = ami.pointer_scale()
    ami.script(('move', int(cx * kx), int(cy * ky)),
               ('button', 1, 1), ('wait', 2), ('button', 1, 0))

def find_win(timeout=10):
    for _ in range(timeout * 2):
        w = ami.window(WIN)
        if w:
            return w
        time.sleep(0.5)
    raise SystemExit('the Prefs window never appeared')

def get(path):
    tmp = os.path.join(HERE, "_prefsrig_get")
    ami.main(['get', path, tmp])
    data = open(tmp, 'rb').read()
    os.unlink(tmp)
    return data.decode('latin-1')

def ui_main():
    binary = os.path.join(HERE, "../../build/amiga/upprefs")
    if not os.path.exists(binary):
        raise SystemExit("build the app first: make build/amiga/upprefs")
    out = os.path.join(HERE, "../../build/rig/shots/prefs_ui.png")
    # a file for Load to read: one profile with a font the field shows
    put('ENV:up-term/up-term',
        b'[profile default]\nfont = TOPAZ:8.8.font\nbell = beep\n')
    ami.main(['put', binary, 'VTC:upprefs'])
    ami.main(['exec', 'run >NIL: VTC:upprefs'])
    w = find_win()
    check("the Prefs window is open", w is not None)
    box = w['box']
    shot(out); _, _, px = pixels(out)
    # the General page is up: both tabs are there, the profile's fields are
    # loaded. Disabled gadgets still appear in the tree, so the page checks
    # that follow are the field's text and what Save writes.
    check("the tabs are there",
          w.get('Colors') is not None and w.get('General') is not None,
          "gadgets: %s" % ", ".join(sorted(k for k in w if k != 'box')))

    # the font field carries the file's value: Load ran on startup
    field = w.get('TOPAZ:8.8.font')
    check("Load filled the font field from the file", field is not None,
          "field=%s" % (field,))
    if field:
        click(field, right=True)
        for _ in range(14):
            ami.key(BACKSPACE)
        ami.main(['type', 'TOPAZ:12.font'])
        time.sleep(1)
        w2 = find_win(2)
        check("the field shows the typed value",
              w2.get('TOPAZ:12.font') is not None,
              "gadgets: %s" % ", ".join(sorted(k for k in w2 if k != 'box')))
        save = w2.get('Save')
        check("the Save button is there", save is not None)
        if save:
            click(save)
            time.sleep(2)
            text = get('ENV:up-term/up-term')
            check("Save wrote the edited font", 'font = TOPAZ:12.font' in text,
                  repr(text[:120]))
            check("Save kept the profile's other keys", 'bell = beep' in text,
                  repr(text[:120]))
            check("Save kept the profile name", '[profile default]' in text)
            try:
                back = get('ENV:up-term/up-term.orig')
                check("the previous file was kept as up-term.orig",
                      'TOPAZ:8.8.font' in back, repr(back[:80]))
            except SystemExit:
                check("the previous file was kept as up-term.orig", False, "no .orig")

    # closing the window: the app must come back with everything written
    try:
        ami.main(['gclick', WIN, 'close'])
    except SystemExit:
        pass
    time.sleep(2)
    check("the window closes", ami.window(WIN) is None)

    for p in ('ENV:up-term/up-term', 'ENV:up-term/up-term.orig', 'VTC:upprefs'):
        try:
            ami.main(['exec', 'Delete %s QUIET' % p])
        except SystemExit:
            pass
    report()

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--ui':
        ui_main()
    else:
        main()
