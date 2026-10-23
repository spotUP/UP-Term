#!/usr/bin/env python3
"""amarchy_gui_rig.py -- the Amarchy drawn menu (amarchy-gui), one reachability test (AM-GUI G7).

Entry point: the Amarchy commodity's hotkey (Left Amiga + Space), typed as a user would; the default
menu style (G6) routes it to amarchy-gui. Plan: thoughts/shared/plans/2026-10-23-amarchy-drawn-menu.md.
Proves, on the rig:
  * the new code RAN: ENV:AMARCHY_DRAWS (gui.c draws_publish, bumped on every full draw) is deleted before the
    run, is above 0 once the menu is open and rises when a level change redraws; amarchy-gui is a running process
  * the window is titled "Amarchy", borderless, and has the size and place that amarchy/menu_geom.c gives for
    this screen and font: tools/rig/amarchy_geom.c is built for the host from the same sources and fed the rig's
    screen size, font and TextLength values (tests/amiga/scrprobe); root level and "apps" level, no numbers
    hardcoded here
  * icons are drawn (G3): the pixels of the first row's icon cell, grabbed from the screen, differ from the
    row's background, in both the selected and the unselected row
  * Esc steps one level back, then closes the window and amarchy-gui exits
  * AMB3: with a second PUBLIC screen in front the menu and the leaf it opens (Apps, UP-Term) appear on it, and
    the menu is sized for that screen's font; with a screen that is NOT public in front they fall back to Workbench
Self-checking. Rig up (rig.py start --max, UPTERM_RIG=<n>), handler installed. Default keymap and menu style
are assumed (no ENV:up-term/keymap override). Run: python3 tools/rig/amarchy_gui_rig.py [--no-build]"""
import os, re, shlex, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c, stage_rig

LAMIGA, SPACE, ESC, RETURN = 0x40, 0x40, 0x45, 0x44
TITLE = 'Amarchy'
PROBE_TITLE = 'G7 screen'      # tests/amiga/scrprobe.c
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
NAMES = ('Amarchy', 'amarchy-menu', 'amarchy-gui', 'scrprobe')
bad = 0


def check(ok, what):
    global bad
    bad += not ok
    print('%s %s' % ('ok' if ok else 'FAIL', what), flush=True)
    return ok


# ---- expected numbers, from the product's own geometry code -------------------------------------------------

def build_geom():
    out = os.path.join(tempfile.mkdtemp(prefix='amarchy_geom'), 'geom')
    src = ['tools/rig/amarchy_geom.c', 'amarchy/menu_geom.c', 'amarchy/menu_core.c', 'amarchy/menu_tree.c',
           'amarchy/menu_icon.c', 'config/keymap.c', 'config/upconf_rows.c', 'render/emoji.c']
    r = subprocess.run(['cc', '-std=c99', '-I', 'build/amiga', '-o', out] + src, cwd=REPO, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('amarchy_geom did not build:\n' + r.stderr[-600:])
    return out


def probe(name):
    """(w, h, ysize, xsize, proportional, rx, ry) of the public screen `name` ('-' = the default one) in the rig:
    the font is the one the menu opens there (amarchy/menu_font.c), rx ry the screen's pixel shape."""
    rc, out = c.run('%s/scrprobe font %s' % (paths.UPTERM_BIN, name))
    m = re.search(r'screen (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)', out)
    if rc or not m:
        raise SystemExit('scrprobe font %s: rc %d %r' % (name, rc, out))
    return tuple(int(v) for v in m.groups())


def expected(geom, name):
    """{'icon': (w, h), 'root'|'apps': {'box': (x, y, w, h), 'cell': (ix, iy, w, h, row2 iy), 'n': n}} for the
    screen `name`: the geometry helper asks for every text it has to measure, scrprobe measures it in that
    screen's own font, and the helper runs again until it asks for nothing more."""
    w, h, ys, xs, prop, rx, ry = probe(name)
    lens = {}
    for _ in range(4):
        r = subprocess.run([geom, str(w), str(h), str(ys), str(xs), str(rx), str(ry)] + ['%s=%d' % kv for kv in lens.items()],
                           capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('amarchy_geom failed: ' + r.stderr)
        need = [l[5:] for l in r.stdout.splitlines() if l.startswith('need ')]
        if not need:
            break
        rc, out = c.run('%s/scrprobe font %s %s' % (paths.UPTERM_BIN, name, ' '.join('"%s"' % t for t in need)))
        for l in out.splitlines():
            m = re.match(r'len (\d+) (.*)$', l)
            if m:
                lens[m.group(2)] = int(m.group(1))
        if not all(t in lens for t in need):
            raise SystemExit('scrprobe did not measure %r: %r' % (need, out))
    else:
        raise SystemExit('amarchy_geom keeps asking for texts')
    e = {'screen': (w, h, ys, xs, prop, rx, ry), 'icon': (0, 0)}
    for l in r.stdout.splitlines():
        f = l.split()
        if f[0] == 'icon':
            e['icon'] = (int(f[1]), int(f[2]))
        elif f[0] == 'level':
            e.setdefault(f[1], {}).update(box=tuple(map(int, f[2:6])), n=int(f[6]))
        elif f[0] == 'cell':
            e[f[1]]['cell'] = tuple(map(int, f[2:7]))
    return e


# ---- the rig's windows, every screen ------------------------------------------------------------------------

def screens():
    with ami._conn() as conn:
        return ami._m().list_windows(conn)[0]


def where(title):
    """(screen, window) of the window titled exactly `title`, front screen first; (None, None) when none."""
    for s in screens():
        for w in s['windows']:
            if w['title'] == title:
                return s, w
    return None, None


def wait_where(title, present=True, secs=15):
    end = time.time() + secs
    while time.time() < end:
        s, w = where(title)
        if bool(w) == present:
            return s, w
        time.sleep(0.5)
    return where(title)


def box_of(s, w):
    """The window's box in Amiga pixels (list_windows reports picture rows: top and height times row_scale)."""
    k = s['row_scale']
    return (w['left'], w['top'] // k, w['width'], w['height'] // k)


def open_menu():
    ami.key(SPACE, LAMIGA)
    return wait_where(TITLE)


def close_window(title):
    """Close a front-screen window by its close gadget (a click on it); True once it is gone."""
    w = ami.window(title)
    if w and 'sys:close' in w:
        x, y, cw, ch = w['sys:close']
        ami.click_px(x + cw // 2, y + ch // 2)
    return not wait_where(title, False, 10)[1]


def process_running(name):
    return any(name in l for l in c.run('Status')[1].splitlines())


def draws():
    rc, out = c.run('GetEnv AMARCHY_DRAWS')
    m = re.match(r'\s*(\d+)\s*$', out) if rc == 0 else None
    return int(m.group(1)) if m else 0


def close_menu():
    for _ in range(3):
        if not where(TITLE)[1]:
            return True
        ami.key(ESC)
        time.sleep(1.5)
    return not where(TITLE)[1]


# ---- pixels -------------------------------------------------------------------------------------------------

def cell_pixels(s, wbox, cell_x, cell_y, cw, ch):
    """(number of pixels in the icon cell that differ from the row background, background RGB). The cell is
    grabbed from the front screen (picture rows: y and height times row_scale); the background is the pixel
    one column left of the cell (inside the row box, row_pad > 0)."""
    k = s['row_scale']
    x0, y0 = wbox[0] + cell_x, (wbox[1] + cell_y) * k
    w, h, rows = ami.grab((x0 - 1, y0, cw + 1, ch * k))
    bg = rows[h // 2][0:3]
    n = 0
    for r in rows:
        for x in range(1, w):
            if r[x * 3:x * 3 + 3] != bg:
                n += 1
    return n, tuple(bg)


# ---- the cases ------------------------------------------------------------------------------------------------

def check_geometry(level, s, w, e, what):
    got = box_of(s, w)
    want = e[level]['box']
    check(got == want, '%s: %s level window %s at %d,%d is %s (want %s from menu_geom.c)' %
          (what, level, '%dx%d' % got[2:], got[0], got[1], got, want))


def case_default(geom):
    e = expected(geom, '-')
    sw, sh, ys, xs, prop, rx, ry = e['screen']
    print('info workbench screen %dx%d pixels %d:%d font %dx%d proportional %d, icon %dx%d' % (sw, sh, rx, ry, xs, ys, prop, e['icon'][0], e['icon'][1]))
    c.run('Delete >NIL: ENV:AMARCHY_DRAWS')
    check(draws() == 0, 'sentinel ENV:AMARCHY_DRAWS deleted before the run')
    check(not where(TITLE)[1] and not process_running('amarchy-gui'), 'no menu window and no amarchy-gui before the hotkey')
    s, w = open_menu()
    if not check(bool(w), 'Super+Space opened a window titled "%s"' % TITLE):
        return
    time.sleep(2)
    s, w = where(TITLE)
    check(process_running('amarchy-gui'), 'amarchy-gui (the drawn menu, not amarchy-menu) is the running process')
    check(not process_running('amarchy-menu'), 'amarchy-menu (the text menu) is not running')
    d0 = draws()
    check(d0 > 0, 'sentinel: ENV:AMARCHY_DRAWS = %d after the menu opened (the drawing code ran)' % d0)
    check(s['front'], 'the window is on the front screen')
    # WA_Borderless leaves no edges and no gadgets; the title bar's height stays as the top border
    bl, bt, br, bb = w['border']
    check(bl == 0 and br == 0 and bb == 0, 'borderless: no left, right or bottom edge (border %s)' % w['border'])
    ui = ami.window(TITLE)
    gad = [k for k in ui if k.startswith('sys:') or k == 'custom']
    check(ui and not gad, 'no gadgets: no close, depth, drag, zoom, size or scroll bar (uitree gadgets %s)' % gad)
    check(e['icon'][0] > 0 and e['icon'][1] > 0, 'an icon column exists (%dx%d)' % e['icon'])
    iw, ih = e['icon']
    check(ry <= rx or iw > ih, 'tall pixels (%d:%d): the icon is wider than high (%dx%d), so it looks square' % (rx, ry, iw, ih))
    check_geometry('root', s, w, e, 'Workbench')
    wbox = box_of(s, w)
    # icons: the first row is selected (SELBOX), the second is not (BG); both icon cells must hold drawn pixels
    ix, iy, cw, ch, iy2 = e['root']['cell']
    n1, bg1 = cell_pixels(s, wbox, ix, iy, cw, ch)
    n2, bg2 = cell_pixels(s, wbox, ix, iy2, cw, ch)
    check(n1 >= 4, 'selected row: icon cell %dx%d holds %d pixels unlike the row background %s' % (cw, ch, n1, bg1))
    check(n2 >= 4, 'plain row: icon cell holds %d pixels unlike the row background %s' % (n2, bg2))
    check(bg1 != bg2, 'selected and plain rows have different backgrounds (%s, %s)' % (bg1, bg2))
    # Enter on the first row (Apps) opens its level: new box, same width, another draw
    ami.key(RETURN)
    time.sleep(2)
    s, w = where(TITLE)
    if check(bool(w), 'Enter on Apps kept the window'):
        check_geometry('apps', s, w, e, 'Workbench')
        check(box_of(s, w)[2] == e['root']['box'][2], 'the width stayed the same while walking a level')
        check(draws() > d0, 'a level change redrew: ENV:AMARCHY_DRAWS %d -> %d' % (d0, draws()))
        ix, iy, cw, ch, iy2 = e['apps']['cell']
        n3, bg3 = cell_pixels(s, box_of(s, w), ix, iy, cw, ch)
        check(n3 >= 4, 'apps level: icon cell holds %d pixels unlike the row background %s' % (n3, bg3))
    # Esc: back to the root level, then closes
    ami.key(ESC)
    time.sleep(2)
    s, w = where(TITLE)
    if check(bool(w), 'Esc in a level stepped back and kept the window'):
        check_geometry('root', s, w, e, 'after Esc')
    ami.key(ESC)
    s, w = wait_where(TITLE, False)
    check(not w, 'Esc at the top level closed the window')
    time.sleep(2)
    check(not process_running('amarchy-gui'), 'amarchy-gui exited after Esc')
    check(process_running('Amarchy'), 'the Amarchy commodity is still running')


def break_probe():
    for l in c.run('Status')[1].splitlines():
        m = re.match(r'\s*Process\s+(\d+):.*scrprobe', l)
        if m:
            c.run('Break %s C' % m.group(1))


def case_screen(geom, public):
    """AMB3: a second screen in front. public: the menu and its leaf follow it; else they stay on Workbench."""
    name = 'G7pub' if public else '-'
    kind = 'public' if public else 'non-public'
    c.run('Run >NIL: %s/scrprobe open %s 240' % (paths.UPTERM_BIN, name))
    end = time.time() + 20
    while time.time() < end and screens()[0]['title'] != PROBE_TITLE:
        time.sleep(0.5)
    if not check(screens()[0]['title'] == PROBE_TITLE, '%s screen "%s" is in front' % (kind, PROBE_TITLE)):
        break_probe()
        return
    try:
        e = expected(geom, name) if public else None
        s, w = open_menu()
        if not check(bool(w), '%s screen in front: the hotkey opened the menu' % kind):
            return
        time.sleep(2)
        s, w = where(TITLE)
        on_probe = s['title'] == PROBE_TITLE
        if public:
            check(on_probe, 'the menu is on the public screen in front ("%s" is on "%s")' % (TITLE, s['title']))
            check_geometry('root', s, w, e, 'public screen')
        else:
            check(not on_probe, 'the menu fell back to Workbench: it is on "%s", not on the non-public "%s"' % (s['title'], PROBE_TITLE))
        # the leaf: Enter on Apps, Enter on UP-Term (the first rows); an XCON window titled UP-Term
        ami.key(RETURN)
        time.sleep(2)
        ami.key(RETURN)
        s2, leaf = wait_where('UP-Term', True, 30)
        if check(bool(leaf), 'the UP-Term leaf opened a window'):
            if public:
                check(s2['title'] == PROBE_TITLE, 'the leaf is on the public screen in front ("%s")' % s2['title'])
            else:
                check(s2['title'] != PROBE_TITLE, 'the leaf fell back to Workbench ("%s")' % s2['title'])
    finally:
        if public:
            close_window('UP-Term')
            close_menu()
            break_probe()
        else:
            break_probe()
            time.sleep(3)
            close_window('UP-Term')
            close_menu()
        end = time.time() + 20
        while time.time() < end and any(x['title'] == PROBE_TITLE for x in screens()):
            time.sleep(0.5)
        check(not any(x['title'] == PROBE_TITLE for x in screens()), '%s screen closed again' % kind)
        check(not where('UP-Term')[1] and not where(TITLE)[1], 'no menu or leaf window left behind (%s case)' % kind)


def main():
    stage_rig.report(stage_rig.stage_build(NAMES, build='--no-build' not in sys.argv))
    geom = build_geom()
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    time.sleep(3)
    case_default(geom)
    case_screen(geom, True)
    case_screen(geom, False)
    c.run('Delete >NIL: ENV:AMARCHY_DRAWS')
    print('FAIL' if bad else 'PASS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
