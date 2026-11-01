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
import paths
import ami
from colour_check import pixels

BGX, BGY = 400, 250
BACKSPACE = 0x41  # (was 0x48: Page Up)
RETURN = 0x44
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
    out = os.path.join(paths.RIG, "shots/prefs_rig.png")
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
# One reachability run (plan 2026-10-14-prefs-ux.md PX15) drives the real
# window the way a user does, with UITREE and clicks, and reads the profile
# files back as text:
#   - every gadget of the table (prefs/prefs_ui.c) is in the window on its page;
#   - Screen = Own screen, Choose... opens asl.library's screen mode
#     requester, a mode and a depth are picked;
#   - a palette entry is set in the colour window (hex field, then the red
#     slider; the slider and the hex field follow each other);
#   - Tab completion = KingCON enables the KingCON gadgets; "List and Tab
#     cycles" and "No beeps" are chosen;
#   - Save: ENV: and ENVARC: hold screen-mode, screen-depth, the palette
#     entry and kingcon-mode = LBS, the keys the editor does not own stay,
#     up-term.orig holds the old file;
#   - Cancel with nothing changed closes at once; the window reopened shows
#     the same values (read from UITREE) and Use writes the same file again;
#   - Cancel with an edit asks "Discard changes?" and Keep editing keeps the window.
# The app is put on the rig as VTC:upprefs (the kit installs it as
# "C:UP-Term-Prefs"; a space in the rig's run command would break the CLI, the
# name itself is Install's business, not the app's). Gadget ids come from
# prefs/prefs_ui.h and the page of each from the table in prefs/prefs_ui.c, so
# a gadget added to the table is checked here without a line changing.

import re, shlex

WIN = 'UP-Term Prefs'
PROFILE = b'[profile default]\nfont = topaz:8\nbell = beep\npalette = 1,AA0000\n'
FILES = ('ENV:up-term/up-term', 'ENV:up-term/up-term.orig', 'ENVARC:up-term/up-term',
         'ENVARC:up-term/up-term.orig')

def read_ids():
    """prefs/prefs_ui.h: PG_X -> number"""
    text = open(os.path.join(HERE, '../../prefs/prefs_ui.h')).read()
    return {m.group(1): int(m.group(2)) for m in re.finditer(r'\b(P[GCN]_[A-Z0-9_]+)\s*=\s*(\d+)', text)}

def read_table(ids):
    """prefs/prefs_ui.c: [(id, page)] with page 0..4, -1 common"""
    text = open(os.path.join(HERE, '../../prefs/prefs_ui.c')).read()
    pages = {'PG_PAGE_GENERAL': 0, 'PG_PAGE_COLORS': 1, 'PG_PAGE_PALETTE': 2, 'PG_PAGE_KEYBOARD': 3,
             'PG_PAGE_ADVANCED': 4, 'PG_COMMON': -1}
    out = []
    for m in re.finditer(r'\{\s*(PG_[A-Z0-9_]+)(?:\s*\+\s*(\d+))?,\s*(PG_PAGE_[A-Z]+|PG_COMMON),\s*PGK_', text):
        out.append((ids[m.group(1)] + int(m.group(2) or 0), pages[m.group(3)]))
    return out

def tree():
    """UITREE: [{'title', 'box', 'g': {id: (box, kind, flags, label, text)}}] for every window"""
    wins = []
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        f = shlex.split(line)
        if line.startswith('W '):
            w, h = map(int, f[4].split('x'))
            wins.append({'title': f[-1], 'box': (int(f[2]), int(f[3]), w, h), 'g': {}})
        elif line.startswith('G ') and wins:
            w, h = map(int, f[4].split('x'))
            gid = int(f[1])
            if f[5] == 'custom':
                # a requester's button: found by its label (its id is the result code, 0 for the last)
                wins[-1]['g'][f[8] if len(f) > 8 else f[7]] = ((int(f[2]), int(f[3]), w, h), f[5], f[6], f[7], '')
            elif f[5].startswith('sys:') or gid == 0:
                wins[-1].setdefault('sys', {})[f[5]] = (int(f[2]), int(f[3]), w, h)
            else:
                # a GadTools gadget is listed more than once (its parts): the first is its box
                wins[-1]['g'].setdefault(gid, ((int(f[2]), int(f[3]), w, h), f[5], f[6],
                                               f[7] if len(f) > 7 else '', f[8] if len(f) > 8 else ''))
    return wins

def win(prefix, nth=0):
    found = [w for w in tree() if w['title'].startswith(prefix)]
    return found[nth] if len(found) > nth else None

def click(box, right=False):
    """a click in a uitree gadget box: at its right end when right (so the
    caret lands after the text of a string gadget), else in the middle"""
    x, y, w, h = box
    cx = x + (w - 4 if right else w // 2)
    cy = y + h // 2
    kx, ky = ami.pointer_scale()
    ami.script(('move', int(cx * kx), int(cy * ky)),
               ('button', 0, 1), ('wait', 2), ('button', 0, 0))  # the left button (1 is the right)

def gad(prefix, gid):
    w = win(prefix)
    return w['g'].get(gid) if w else None

def click_gad(prefix, gid, right=False, settle=0.7):
    g = gad(prefix, gid)
    if g is None:
        raise SystemExit('no gadget %d in %s' % (gid, prefix))
    click(g[0], right)
    time.sleep(settle)

def wait_win(prefix, timeout=10, gone=False):
    for _ in range(int(timeout * 2)):
        if (win(prefix) is None) == gone:
            return win(prefix)
        time.sleep(0.5)
    return None if not gone else win(prefix)

def get(path):
    tmp = os.path.join(HERE, "_prefsrig_get")
    try:
        ami.main(['get', path, tmp])
    except SystemExit:
        return None
    data = open(tmp, 'rb').read()
    os.unlink(tmp)
    return data.decode('latin-1')

def goto_page(cur, want):
    """the Page cycle moves one page a click; returns the page now shown"""
    pg = ID['PG_PAGE']
    while cur != want:
        click_gad(WIN, pg)
        cur = (cur + 1) % 5
    return cur

def start_app():
    ami.main(['exec', 'run >NIL: VTC:upprefs'])
    w = wait_win(WIN)
    if not w:
        raise SystemExit('the Prefs window never appeared')
    time.sleep(1)
    return w

ID = {}

def ui_main():
    global ID
    binary = os.path.join(HERE, "../../build/amiga/upprefs")
    if not os.path.exists(binary):
        raise SystemExit("build the app first: make build/amiga/upprefs")
    ID = read_ids()
    table = read_table(ID)
    check("the gadget table was read (ids and pages)", len(table) > 80, "%d gadgets" % len(table))
    ensure_dir()
    saved = {p: ami.read_file(p) for p in FILES}
    for d in ('ENV:up-term', 'ENVARC:up-term'):
        try:
            ami.main(['exec', 'MakeDir %s' % d])
        except SystemExit:
            pass
    for p in FILES:
        ami.restore_file(p, None)
    put('ENV:up-term/up-term', PROFILE)
    ami.main(['put', binary, 'VTC:upprefs'])
    try:
        ui_run(table)
    finally:
        if win(WIN):
            w = win(WIN)
            c = w.get('sys', {}).get('sys:close')
            if c:
                click(c)
                time.sleep(1)
        for p in FILES:
            ami.restore_file(p, saved[p])
        try:
            ami.main(['exec', 'Delete VTC:upprefs QUIET'])
        except SystemExit:
            pass
    report()

def ui_run(table):
    w = start_app()
    check("the Prefs window is open", w is not None)
    # 1. every gadget of the table is in the window, on its page
    cur = 0
    for page in range(5):
        cur = goto_page(cur, page)
        w = win(WIN)
        want = [i for i, p in table if p in (page, -1)]
        missing = [i for i in want if i not in w['g']]
        check("page %d: all %d gadgets of the table are in the window" % (page, len(want)), not missing,
              "missing %s" % missing[:8])
    # 2. a screen mode from asl.library's requester
    cur = goto_page(cur, 4)
    check("Choose... is greyed out while Screen is Workbench",
          'disabled' in gad(WIN, ID['PG_SCREEN_CHOOSE'])[2])
    click_gad(WIN, ID['PG_SCREEN'])                     # Workbench -> Own screen
    check("Choose... is enabled for an own screen", 'disabled' not in gad(WIN, ID['PG_SCREEN_CHOOSE'])[2])
    click_gad(WIN, ID['PG_SCREEN_CHOOSE'], settle=2)
    req = win('UP-Term screen mode')
    check("the screen mode requester opens", req is not None)
    if req:
        lst = req['g'][3][0]
        ami.click_px(lst[0] + 60, lst[1] + 30)          # a mode of the list
        time.sleep(0.8)
        click_gad('UP-Term screen mode', 1, settle=1.5)  # OK
    check("the requester closed", win('UP-Term screen mode') is None)
    # 2b. a font from the font requester; the fallback font and theme requesters open
    cur = goto_page(cur, 0)
    click_gad(WIN, ID['PG_FONT_CHOOSE'], settle=2.5)
    check("the font requester opens", win('UP-Term font') is not None)
    if win('UP-Term font'):
        click_gad('UP-Term font', 6, right=True, settle=0.4)       # its Size field
        for _ in range(4):
            ami.key(BACKSPACE)
        ami.main(['type', '9'])
        time.sleep(0.5)
        click_gad('UP-Term font', 1, settle=2)                      # OK
    check("the font requester closed", win('UP-Term font') is None)
    click_gad(WIN, ID['PG_FALLBACK_CHOOSE'], settle=2.5)
    r = win('Choose an outline font')
    check("the fallback font requester opens", r is not None)
    if r:
        click(r['sys']['sys:close'])
        time.sleep(1.5)
    check("the fallback font requester closed", win('Choose an outline font') is None)
    cur = goto_page(cur, 1)
    click_gad(WIN, ID['PG_THEME'], settle=2.5)
    r = win('Choose a theme')
    check("the theme requester opens", r is not None)
    if r:
        click(r['sys']['sys:close'])
        time.sleep(1.5)
    check("the theme requester closed", win('Choose a theme') is None)
    # 3. a palette entry in the colour window: hex field, then the red slider
    cur = goto_page(cur, 2)
    click_gad(WIN, ID['PG_PAL'] + 1, settle=2)
    cw = win('Colour')
    check("the colour window opens on the swatch", cw is not None and cw['g'].get(ID['PC_HEX'], [0, 0, 0, 0, ''])[4] == 'AA0000',
          str(cw['g'].get(ID['PC_HEX']) if cw else None))
    # the wheel is BOOPSI and not in UITREE: the title names it only when
    # NewObject made it (colorwheel.gadget and gradientslider.gadget opened)
    check("the colour window has a colour wheel (title says so)",
          cw is not None and 'colour wheel' in cw['title'], cw['title'] if cw else None)
    if cw:
        click_gad('Colour', ID['PC_HEX'], right=True, settle=0.5)
        for _ in range(8):
            ami.key(BACKSPACE)
        ami.main(['type', '12AB34'])
        time.sleep(0.5)
        ami.key(RETURN)
        time.sleep(1)
        check("the hex field takes 12AB34", gad('Colour', ID['PC_HEX'])[4] == '12AB34', str(gad('Colour', ID['PC_HEX'])))
        # the red slider's track near its right end: one step up a click
        red = gad('Colour', ID['PC_RED'])[0]
        for _ in range(2):
            ami.click_px(red[0] + red[2] - 6, red[1] + red[3] // 2)
            time.sleep(0.5)
        check("the red slider moved the hex field to 14AB34", gad('Colour', ID['PC_HEX'])[4] == '14AB34',
              str(gad('Colour', ID['PC_HEX'])))
        click_gad('Colour', ID['PC_OK'], settle=1.5)
    check("the colour window closed", win('Colour') is None)
    check("the palette entry's hex field shows 14AB34", gad(WIN, ID['PG_PAL_HEX'] + 1)[4] == '14AB34',
          str(gad(WIN, ID['PG_PAL_HEX'] + 1)))
    # 4. KingCON: the gadgets are greyed until Tab completion is KingCON
    cur = goto_page(cur, 3)
    check("KingCON gadgets are greyed out for Unix completion",
          all('disabled' in gad(WIN, ID[n])[2] for n in ('PG_KC_MATCH', 'PG_KC_COMMON', 'PG_KC_SILENT',
                                                         'PG_KC_INFO', 'PG_KC_CACHE')))
    click_gad(WIN, ID['PG_COMPLETE'])
    check("KingCON gadgets are enabled by Tab completion = KingCON",
          all('disabled' not in gad(WIN, ID[n])[2] for n in ('PG_KC_MATCH', 'PG_KC_COMMON', 'PG_KC_SILENT',
                                                             'PG_KC_INFO', 'PG_KC_CACHE')))
    for _ in range(3):                                   # Window, List, Tab cycles, List and Tab cycles
        click_gad(WIN, ID['PG_KC_MATCH'], settle=0.5)
    click_gad(WIN, ID['PG_KC_SILENT'])
    check("No beeps is checked", 'selected' in gad(WIN, ID['PG_KC_SILENT'])[2])
    # 5. Save writes both files and ends the editor
    click_gad(WIN, ID['PG_SAVE'], settle=3)
    check("Save ends the editor", wait_win(WIN, 5, gone=True) is None)
    env = get('ENV:up-term/up-term') or ''
    arc = get('ENVARC:up-term/up-term') or ''
    for name, text in (('ENV:', env), ('ENVARC:', arc)):
        check("%s holds screen-mode = 0x........" % name, re.search(r'(?m)^screen-mode = 0x[0-9a-f]{8}$', text) is not None,
              repr(text[:200]))
        check("%s holds screen-depth, screen = own" % name,
              re.search(r'(?m)^screen-depth = [4-8]$', text) is not None and 'screen = own' in text)
        check("%s holds the palette entry 14AB34" % name, '01,14AB34' in text)
        check("%s holds kingcon-mode = LBS, completion = kingcon" % name,
              'kingcon-mode = LBS' in text and 'completion = kingcon' in text)
        check("%s holds the font from the requester, font = topaz:9" % name, re.search(r'(?m)^font = topaz:9$', text) is not None)
        check("%s kept the keys the editor does not own" % name, 'bell = beep' in text)
    back = get('ENV:up-term/up-term.orig') or ''
    check("the previous file was kept as up-term.orig", 'palette = 1,AA0000' in back, repr(back[:100]))
    # 6. the reopened window shows the same, and Use writes the same file and ends
    start_app()
    cur = 0
    cur = goto_page(cur, 2)
    check("reopened: the palette entry shows 14AB34", gad(WIN, ID['PG_PAL_HEX'] + 1)[4] == '14AB34')
    cur = goto_page(cur, 3)
    check("reopened: KingCON is on and No beeps is checked",
          'disabled' not in gad(WIN, ID['PG_KC_MATCH'])[2] and 'selected' in gad(WIN, ID['PG_KC_SILENT'])[2])
    cur = goto_page(cur, 4)
    check("reopened: the screen mode can be chosen (Own screen)",
          'disabled' not in gad(WIN, ID['PG_SCREEN_CHOOSE'])[2])
    click_gad(WIN, ID['PG_USE'], settle=3)
    check("Use ends the editor", wait_win(WIN, 5, gone=True) is None)
    again = get('ENV:up-term/up-term') or ''
    check("reopened and used again: the file is the same", again == env, repr(again[:200]))
    # 7. Cancel with an edit: nothing asked, nothing written, the file byte for byte; the
    #    close gadget with an edit asks "Discard changes?"
    start_app()
    cur = 0
    click_gad(WIN, ID['PG_SCROLLBAR'])
    click_gad(WIN, ID['PG_CANCEL'], settle=2)
    check("Cancel with an edit ends the editor without asking", win(WIN) is None and
          not [x for x in tree() if x['title'].startswith('UP-Term Prefs')])
    check("Cancel left ENV: byte for byte", (get('ENV:up-term/up-term') or '') == env)
    check("Cancel left ENVARC: byte for byte", (get('ENVARC:up-term/up-term') or '') == arc)
    start_app()
    cur = 0
    click_gad(WIN, ID['PG_SCROLLBAR'])
    click(win(WIN)['sys']['sys:close'])
    time.sleep(2)
    ask = [x for x in tree() if x['title'].startswith('UP-Term Prefs') and x['box'][2] < 400]
    check("closing the window with an edit asks (a request opens)", len(ask) == 1)
    if ask:
        click(ask[0]['g']['Keep editing'][0])
        time.sleep(1.5)
    check("Keep editing keeps the window", win(WIN) is not None and
          not [x for x in tree() if x['title'].startswith('UP-Term Prefs') and x['box'][2] < 400])
    click(win(WIN)['sys']['sys:close'])
    time.sleep(2)
    ask = [x for x in tree() if x['title'].startswith('UP-Term Prefs') and x['box'][2] < 400]
    if ask:
        click(ask[0]['g']['Discard'][0])
        time.sleep(2)
    check("Discard closes the window", win(WIN) is None)
    check("Discard wrote nothing", (get('ENV:up-term/up-term') or '') == env)
    # 7b. closing a window nothing was edited in asks nothing
    start_app()
    click(win(WIN)['sys']['sys:close'])
    check("closing an unedited window asks nothing", wait_win(WIN, 5, gone=True) is None)
    # 8. profiles: New..., Rename..., Delete (asks) -- each written by Use, which ends the editor
    def name_window(text, initial_len):
        """the one-field window: erase what is in it, type, Return"""
        n = wait_win('New profile', 3) or wait_win('Rename profile', 3)
        if not n:
            return None
        title = n['title']
        for _ in range(initial_len):
            ami.key(BACKSPACE)
        ami.main(['type', text])
        time.sleep(0.5)
        ami.key(RETURN)
        wait_win(title, 5, gone=True)
        return title

    def profiles_in_file():
        return re.findall(r'(?m)^\[profile ([^\]]+)\]', get('ENV:up-term/up-term') or '')

    def use_and_wait():
        click_gad(WIN, ID['PG_USE'], settle=3)
        wait_win(WIN, 5, gone=True)

    start_app()
    click_gad(WIN, ID['PG_PROFILE_NEW'], settle=1.5)
    check("New... opens the name window", name_window('work', 0) == 'New profile')
    use_and_wait()
    check("Use wrote the new profile beside default", profiles_in_file() == ['default', 'work'], str(profiles_in_file()))
    start_app()
    click_gad(WIN, ID['PG_PROFILE'])                       # default -> work
    click_gad(WIN, ID['PG_PROFILE_RENAME'], settle=1.5)
    check("Rename... opens the name window", name_window('office', 4) == 'Rename profile')
    use_and_wait()
    check("Rename wrote office and dropped work", profiles_in_file() == ['default', 'office'], str(profiles_in_file()))
    start_app()
    click_gad(WIN, ID['PG_PROFILE'])                       # default -> office
    click_gad(WIN, ID['PG_PROFILE_DELETE'], settle=1.5)
    ask = [x for x in tree() if x['title'].startswith('UP-Term Prefs') and x['box'][2] < 400]
    check("Delete asks first", len(ask) == 1 and 'Delete' in ask[0]['g'])
    if ask:
        click(ask[0]['g']['Delete'][0])
        time.sleep(1.5)
    use_and_wait()
    check("Delete removed office, default stays", profiles_in_file() == ['default'], str(profiles_in_file()))
    # 9. an open UP-Term window takes the new colour after Use, without reopening
    put('ENV:up-term/up-term', b'[profile default]\nbell = beep\n')  # on the Workbench, not the own screen above
    open_win('prefsLive', 'PROFILE default')
    out = os.path.join(paths.RIG, "shots/prefs_live.png")
    shot(out); _, _, px = pixels(out)
    check("the open window's background starts black", fam(px(BGX, BGY)) == 'b', str(px(BGX, BGY)))
    start_app()
    cur = goto_page(0, 1)
    click_gad(WIN, ID['PG_BG'], settle=2)
    if win('Colour'):
        click_gad('Colour', ID['PC_HEX'], right=True, settle=0.5)
        for _ in range(8):
            ami.key(BACKSPACE)
        ami.main(['type', 'FF0000'])
        time.sleep(0.5)
        ami.key(RETURN)
        time.sleep(1)
        click_gad('Colour', ID['PC_OK'], settle=1.5)
    use_and_wait()
    time.sleep(4)                                   # the window's watcher reads the file
    shot(out); _, _, px = pixels(out)
    check("the open window's background turned red after Use", fam(px(BGX, BGY)) == 'r', str(px(BGX, BGY)))
    lw = ami.window('prefsLive')
    if lw and 'sys:close' in lw:
        ami.click_px(lw['sys:close'][0] + 8, lw['sys:close'][1] + 5)
        time.sleep(2)
    check("the machine is still up", run_cmd('Echo up') == 'up')


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
