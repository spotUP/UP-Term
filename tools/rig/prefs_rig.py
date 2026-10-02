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
    """the first pixel row at/below y0 with ink of family want near x. One
    row at a time: ink_at's +-4 rows let the next text line's brighter ink
    (yellow over red) win, so a red line was never seen (2026-10-02)"""
    for y in range(y0, y1):
        for dx in range(-4, 12):
            if fam(px(x + dx, y)) == want:
                return y
    return None

def next_row(px, x, y, y1, want):
    """the top of the next text row of family want: past the ink of the row
    starting at y and a blank pixel row, the first ink again (a glyph is
    several pixel rows tall, so "y + 4" landed in the same row)"""
    def ink(yy):
        return any(fam(px(x + dx, yy)) == want for dx in range(-4, 12))
    while y < y1 and ink(y):
        y += 1
    while y < y1 and not ink(y):
        y += 1
    return y if y < y1 else None


def open_win(title, opts=""):
    # XCON:x/y/w/h/TITLE/OPT... -- the title is the fifth field, so it has to
    # come before the options; passing "PROFILE a" there made it the title and
    # the profile was never applied.
    ami.req(0x02, struct.pack('>H', 30) +
            ('run >NIL: newshell "XCON:0/12/640/300/%s/CLOSE%s"'
             % (title, ("/" + opts if opts else ""))).encode('latin-1'))
    time.sleep(4)
    for _ in range(3):
        w = ami.window(title)
        if w:
            return w
        time.sleep(2)
    raise SystemExit("window %r never appeared" % title)

def shot(out):
    ami.main(['shot', out]); SHOTS.append(out)

def ensure_dir():
    # the drawer the kit makes; absent on a rig without the kit (install_rig
    # uninstalls it)
    b = ami.req(0x02, struct.pack('>H', 10) + b'MakeDir >NIL: ENV:up-term')


def main():
    ensure_dir()
    # two profiles, and no "default" section: a window without PROFILE <name>
    # falls back to the built-in look (light text on black).
    put('ENV:up-term/up-term',
        b'[profile a]\nfg = FFFF00\nbg = 000000\npalette = 2,FF0000\n'
        b'selection-bg = 0000FF\nselection-fg = FFFFFF\n'
        b'[profile b]\nfg = 00FFFF\nbg = FF0000\nfont = TOPAZ:16\n')
    # the bytes to Type: an ANSI-green word and a plain word, one per line
    put('VTC:prefsa.txt', b'\x1b[32mGGGG\x1b[0m\nHHHH\n')
    put('VTC:prefsb.txt', b'WWWW\nWWWW\n')
    out = os.path.join(HERE, "../../build/rig/shots/prefs_rig.png")
    x = 20   # inside the window's first column area
    # a background pixel: well right of and below any text the test writes
    # (x=20,y=100 sat on the prompt line once profile b's 16-pixel font
    # pushed the rows down)

    # 1: no profile -> the built-in defaults: black bg, light achromatic text
    open_win('prefs0')
    ami.main(['type', 'Type VTC:prefsb.txt'])
    time.sleep(0.4)
    ami.key(0x44)  # Return: the command runs (it only sat on the line)
    time.sleep(3)
    shot(out); _, _, px = pixels(out)
    bg = fam(px(BGX, BGY))
    check("no-profile background is black", bg == 'b', "fam=%s" % bg)
    shot(out.replace('.png', '-0.png'))
    ty = scan_ink_row(px, x, 30, 200, 'g', (0, 0, 0))
    check("no-profile text is light (built-in grey)", ty is not None, "y=%s" % ty)

    # 2 + 3: profile a: yellow on black, ANSI green remapped to red
    open_win('prefsA', 'PROFILE a')
    ami.main(['type', 'Type VTC:prefsa.txt'])
    time.sleep(0.4)
    ami.key(0x44)  # Return: the command runs (it only sat on the line)
    time.sleep(3)
    shot(out); _, _, px = pixels(out)
    shot(out.replace('.png', '-a.png'))
    check("profile a background is black", fam(px(BGX, BGY)) == 'b')
    yg = scan_ink_row(px, x, 30, 200, 'r', (0, 0, 0))   # GGGG: remapped green
    yh = next_row(px, x, yg, 220, 'y') if yg else None  # HHHH: the next text row
    check("profile a palette: SGR 32 draws red", yg is not None, "y=%s" % yg)
    check("profile a foreground is yellow", yh is not None, "y=%s" % yh)
    step_a = (yh - yg) if (yg and yh) else None

    # 3b: profile a's selection: a drag over the text paints it with the
    # profile's selection colours instead of flipping the cell
    w = ami.window('prefsA')
    if not w:
        check("selection colours (no window to drag in)", False)
    else:
        bx, by, bw, bh = w['box']
        kx, ky = ami.pointer_scale()
        # inside the first text row, from the first column across two cells
        sx, sy = bx + 22, by + 40
        ex, ey = bx + 90, by + 40
        ami.script(('move', int(sx * kx), int(sy * ky)),
                   ('button', 0, 1),
                   ('move', int(ex * kx), int(ey * ky)),
                   ('wait', 4),
                   ('button', 0, 0))
        time.sleep(2)
        shot(out); _, _, px = pixels(out)
        # the selected cell sits under the drag; the row below it holds the
        # same text unselected, so the two must differ
        sel = px(bx + 40, by + 40)
        unsel = px(bx + 40, by + 56)
        check("drag-select paints the profile's selection background",
              sel[2] > 100 and sel[2] > sel[0] + 40,
              "selected rgb=%s, below it rgb=%s" % (sel, unsel))

    # 4: profile b: cyan on red, topaz 16 -> the row pitch doubles
    open_win('prefsB', 'PROFILE b')
    ami.main(['type', 'Type VTC:prefsb.txt'])
    time.sleep(0.4)
    ami.key(0x44)  # Return: the command runs (it only sat on the line)
    time.sleep(3)
    shot(out); _, _, px = pixels(out)
    shot(out.replace('.png', '-b.png'))
    check("profile b background is red", fam(px(BGX, BGY)) == 'r', "rgb=%s" % (px(BGX, BGY),))
    w1 = scan_ink_row(px, x, 30, 220, 'c', (255, 0, 0))
    w2 = next_row(px, x, w1, 260, 'c')
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

BGX, BGY = 400, 250
BACKSPACE = 0x41  # (was 0x48: Page Up)
WIN = 'UP-Term Prefs'

def click(box, right=False):
    """a click in a uitree gadget box: at its right end when right (so the
    caret lands after the text of a string gadget), else in the middle"""
    x, y, w, h = box
    cx = x + (w - 4 if right else w // 2)
    cy = y + h // 2
    kx, ky = ami.pointer_scale()
    ami.script(('move', int(cx * kx), int(cy * ky)),
               ('button', 0, 1), ('wait', 2), ('button', 0, 0))  # the left button (1 is the right)

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

def gadgets():
    """the Prefs window's gadgets from UITREE: id -> (box, kind, label, text).
    GadTools buttons show no label there; they are found by GadgetID
    (prefs/upprefs.c: ID_SAVE 36, ID_USE 37, ID_CANCEL 38)"""
    import shlex
    out, inside = {}, False
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        if line.startswith('W '):
            if inside:
                break
            inside = shlex.split(line)[-1] == WIN
        elif inside and line.startswith('G '):
            f = shlex.split(line)
            w, h = map(int, f[4].split('x'))
            out[int(f[1])] = ((int(f[2]), int(f[3]), w, h), f[5], f[7] if len(f) > 7 else '',
                              f[8] if len(f) > 8 else '')
    return out


def by_label(g, label):
    for gid, v in g.items():
        if v[2] == label:
            return gid, v
    return None, None


ID_SAVE = 36


def ui_main():
    binary = os.path.join(HERE, "../../build/amiga/upprefs")
    if not os.path.exists(binary):
        raise SystemExit("build the app first: make build/amiga/upprefs")
    out = os.path.join(HERE, "../../build/rig/shots/prefs_ui.png")
    ensure_dir()
    # a file for Load to read: one profile with a font the field shows
    put('ENV:up-term/up-term',
        b'[profile default]\nfont = TOPAZ:8.8.font\nbell = beep\n')
    ami.main(['put', binary, 'VTC:upprefs'])
    ami.main(['exec', 'run >NIL: VTC:upprefs'])
    w = find_win()
    check("the Prefs window is open", w is not None)
    shot(out)
    g = gadgets()
    check("the page switch is there", by_label(g, 'Page')[0] is not None)
    fid, font = by_label(g, 'Font')
    check("Load filled the font field from the file", font is not None and font[3] == 'TOPAZ:8.8.font',
          "font=%s" % (font,))
    if font:
        click(font[0], right=True)
        for _ in range(16):
            ami.key(BACKSPACE)
        ami.main(['type', 'TOPAZ:12.font'])
        time.sleep(1)
        font2 = gadgets().get(fid)
        check("the field shows the typed value", font2 is not None and font2[3] == 'TOPAZ:12.font',
              "font=%s" % (font2,))
        save = gadgets().get(ID_SAVE)
        check("the Save button is there", save is not None)
        if save:
            click(save[0])
            time.sleep(2)
            text = get('ENV:up-term/up-term')
            check("Save wrote the edited font", 'font = TOPAZ:12.font' in text, repr(text[:120]))
            check("Save kept the profile's other keys", 'bell = beep' in text, repr(text[:120]))
            check("Save kept the profile name", '[profile default]' in text)
            try:
                back = get('ENV:up-term/up-term.orig')
                check("the previous file was kept as up-term.orig",
                      'TOPAZ:8.8.font' in back, repr(back[:80]))
            except SystemExit:
                check("the previous file was kept as up-term.orig", False, "no .orig")
            try:
                arc = get('ENVARC:up-term/up-term')
                check("Save also wrote ENVARC:", 'font = TOPAZ:12.font' in arc, repr(arc[:80]))
            except SystemExit:
                check("Save also wrote ENVARC:", False, "none")

    # closing the window: the app must end and the machine keep running
    # (closing with a loaded file crashed it: a double FreeGadgets)
    if ami.window(WIN):
        ami.main(['gclick', WIN, 'sys:close'])
    time.sleep(3)
    check("the window closes", ami.window(WIN) is None)
    check("the machine is still up", run_cmd('Echo up') == 'up')

    for p in ('ENV:up-term/up-term', 'ENV:up-term/up-term.orig', 'ENVARC:up-term/up-term.orig', 'VTC:upprefs'):
        try:
            ami.main(['exec', 'Delete %s QUIET' % p])
        except SystemExit:
            pass
    report()


def run_cmd(cmd):
    try:
        b = ami.req(0x02, struct.pack('>H', 10) + cmd.encode('latin-1'))
        return b[4:].decode('latin-1').strip()
    except Exception:
        return ''

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--ui':
        ui_main()
    else:
        main()
