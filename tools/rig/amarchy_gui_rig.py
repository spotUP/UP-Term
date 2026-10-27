#!/usr/bin/env python3
"""amarchy_gui_rig.py -- the Amarchy drawn menu (amarchy-gui), one reachability test (AM-GUI G7).

Entry point: the Amarchy commodity's hotkey (Left Amiga + Space), typed as a user would; the default
menu style (G6) routes it to amarchy-gui. Plan: thoughts/shared/plans/2026-10-23-amarchy-drawn-menu.md.
Proves, on the rig:
  * the menu is drawn offscreen and blitted (no flicker when the bar moves): ENV:AMARCHY_BLITS is above 0 once
    the menu is open and rises with a level change
  * the new code RAN: ENV:AMARCHY_DRAWS (gui.c draws_publish, bumped on every full draw) is deleted before the
    run, is above 0 once the menu is open and rises when a level change redraws; amarchy-gui is a running process
  * the window is titled "Amarchy", borderless, and has the size and place that amarchy/menu_geom.c gives for
    this screen and font: tools/rig/amarchy_geom.c is built for the host from the same sources and fed the rig's
    screen size, font and TextLength values (tests/amiga/scrprobe); root level and "apps" level, no numbers
    hardcoded here
  * the menu's font: JetBrainsMono 16 on a square-pixel screen, topaz 8 on tall pixels (640x256 PAL hires);
    JetBrainsMono.font and its 16 drawer are staged from build/jbmono/out into a drawer added to FONTS:
  * icons are drawn (G3): the pixels of the first row's icon cell, grabbed from the screen, differ from the
    row's background, in both the selected and the unselected row
  * Esc steps one level back, then closes the window and amarchy-gui exits
  * Packages > Search keeps the menu open after Return and after Escape at the prompt (--pkg-search runs only this)
  * AMB3: with a second PUBLIC screen in front the menu and the leaf it opens (Apps, UP-Term) appear on it, and
    the menu is sized for that screen's font; with a screen that is NOT public in front they fall back to Workbench
Self-checking. Rig up (rig.py start --max, UPTERM_RIG=<n>), handler installed. Default keymap and menu style
are assumed (no ENV:up-term/keymap override). Run: python3 tools/rig/amarchy_gui_rig.py [--setup] [--no-build]
--setup first prepares the rig by itself (kills the kit Amarchy, puts the build to VTCX:Apps/bin, assigns UP-Term:
and starts that Amarchy); alone it runs only that. --help prints this text."""
import hashlib, os, re, shlex, shutil, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c, stage_rig, amarchy_window_rig

LAMIGA, SPACE, ESC, RETURN, DOWN = 0x40, 0x40, 0x45, 0x44, 0x4D
TITLE = 'Amarchy'
PROBE_TITLE = 'G7 screen'      # tests/amiga/scrprobe.c
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
NAMES = ('Amarchy', 'amarchy-menu', 'amarchy-gui', 'scrprobe')
bad = 0
FONTS_OPENED = {}            # screen name -> (font name, ysize) the menu's font code opened (scrprobe)
FONT_DRAWER = 'amarchy-fonts'
JBMONO = os.path.join(REPO, 'build/jbmono/out')
SETUP_NAMES = ('Amarchy', 'amarchy-menu', 'amarchy-gui')
SETUP_BIN = 'VTCX:Apps/bin'


def check(ok, what):
    global bad
    bad += not ok
    print('%s %s' % ('ok' if ok else 'FAIL', what), flush=True)
    return ok


# ---- expected numbers, from the product's own geometry code -------------------------------------------------

def build_geom():
    out = os.path.join(tempfile.mkdtemp(prefix='amarchy_geom'), 'geom')
    src = ['tools/rig/amarchy_geom.c', 'amarchy/menu_geom.c', 'amarchy/menu_core.c', 'amarchy/menu_tree.c',
           'amarchy/menu_icon.c', 'config/keymap.c', 'config/upconf_rows.c', 'render/emoji.c', 'prefs/settings_def.c', 'prefs/prefs_core.c', 'config/upconf.c', 'handler/kcmode.c', 'handler/slash.c', 'amarchy/amconf_keys.c']
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
    FONTS_OPENED[name] = re.search(r'fontname (\S+) (\d+)', out).groups()
    return tuple(int(v) for v in m.groups())


def check_font(name, e, what):
    """Square pixels: JetBrainsMono 16 (render/fontpair.c fontpair_menu_choices); tall pixels: topaz 8."""
    got = FONTS_OPENED[name]
    sw, sh, ys, xs, prop, rx, ry = e['screen']
    if rx == ry:
        ok = got[0].lower().startswith('jetbrainsmono') and int(got[1]) == 16
        want = 'JetBrainsMono 16'
    else:
        ok = got[0].lower().startswith('topaz') and int(got[1]) == 8
        want = 'topaz 8'
    check(ok, '%s: pixels %d:%d, the menu opens font %s %s (want %s)' % (what, rx, ry, got[0], got[1], want))


def stage_fonts():
    """JetBrainsMono.font + JetBrainsMono/16 into <rig>/vtc/amarchy-fonts, added to FONTS: for the run."""
    if not os.path.isfile(os.path.join(JBMONO, 'JetBrainsMono.font')):
        subprocess.run(['make', 'jbmono'], cwd=REPO, check=True)
    dst = os.path.join(str(paths.RIG), 'vtc', FONT_DRAWER)
    shutil.rmtree(dst, ignore_errors=True)
    os.makedirs(os.path.join(dst, 'JetBrainsMono'))
    shutil.copy(os.path.join(JBMONO, 'JetBrainsMono.font'), dst)
    shutil.copy(os.path.join(JBMONO, 'JetBrainsMono/16'), os.path.join(dst, 'JetBrainsMono/16'))
    c.run('Assign FONTS: VTC:%s ADD' % FONT_DRAWER)


def expected(geom, name, wb=False):
    """{'icon': (w, h), 'root'|'apps': {'box': (x, y, w, h), 'cell': (ix, iy, w, h, row2 iy), 'n': n}} for the
    screen `name`: the geometry helper asks for every text it has to measure, scrprobe measures it in that
    screen's own font, and the helper runs again until it asks for nothing more."""
    w, h, ys, xs, prop, rx, ry = probe(name)
    lens = {}
    for _ in range(4):
        r = subprocess.run([geom] + (['--wb'] if wb else []) + [str(w), str(h), str(ys), str(xs), str(rx), str(ry)] + ['%s=%d' % kv for kv in lens.items()],
                           capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('amarchy_geom failed: ' + r.stderr)
        need = [l[5:] for l in r.stdout.splitlines() if l.startswith('need ')]
        if not need:
            break
        # wasabid holds 511 bytes of command: the texts go in chunks (the Workbench group has 42 labels)
        chunks, cur = [], []
        for t in need:
            if cur and len(' '.join('"%s"' % x for x in cur + [t])) > 300:
                chunks.append(cur)
                cur = []
            cur.append(t)
        chunks.append(cur)
        for chunk in chunks:
            rc, out = c.run('%s/scrprobe font %s %s' % (paths.UPTERM_BIN, name, ' '.join('"%s"' % t for t in chunk)))
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
        elif f[0] == 'row':
            e.setdefault('rows', {})[f[1]] = int(f[2])
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


def env_count(name):
    rc, out = c.run('GetEnv ' + name)
    m = re.match(r'\s*(\d+)\s*$', out) if rc == 0 else None
    return int(m.group(1)) if m else 0


def draws():
    return env_count('AMARCHY_DRAWS')


def blits():
    """ENV:AMARCHY_BLITS: back buffer to window copies (gui.c paint_end), published with every full draw"""
    return env_count('AMARCHY_BLITS')


def close_menu():
    for _ in range(3):
        if not where(TITLE)[1]:
            return True
        ami.key(ESC)
        time.sleep(1.5)
    return not where(TITLE)[1]


# ---- pixels -------------------------------------------------------------------------------------------------

def cell_grid(s, wbox, cell_x, cell_y, cw, ch):
    """(rows, background RGB) of the icon cell: the cell grabbed from the front screen, each row cw*3 bytes of
    RGB; the background is the pixel one column left of the cell (inside the row box, row_pad > 0). ami.grab
    returns the screen's own rows (the legacy shot is raw, not row-doubled), and wbox is in screen rows."""
    w, h, rows = ami.grab((wbox[0] + cell_x - 1, wbox[1] + cell_y, cw + 1, ch))
    return [bytes(r[3:]) for r in rows], tuple(rows[h // 2][0:3])


def cell_pixels(s, wbox, cell_x, cell_y, cw, ch):
    """(number of pixels in the icon cell that differ from the row background, background RGB)"""
    rows, bg = cell_grid(s, wbox, cell_x, cell_y, cw, ch)
    n = sum(1 for r in rows for x in range(len(r) // 3) if r[x * 3:x * 3 + 3] != bytes(bg))
    return n, bg


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
    c.run('Delete >NIL: ENV:AMARCHY_DRAWS ENV:AMARCHY_BLITS')
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
    b0 = blits()
    check(b0 > 0, 'sentinel: ENV:AMARCHY_BLITS = %d, the menu is drawn offscreen and blitted (a moving bar does not flicker)' % b0)
    check(s['front'], 'the window is on the front screen')
    # WA_Borderless leaves no edges and no gadgets; the title bar's height stays as the top border
    bl, bt, br, bb = w['border']
    check(bl == 0 and br == 0 and bb == 0, 'borderless: no left, right or bottom edge (border %s)' % w['border'])
    ui = ami.window(TITLE)
    gad = [k for k in ui if k.startswith('sys:') or k == 'custom']
    check(ui and not gad, 'no gadgets: no close, depth, drag, zoom, size or scroll bar (uitree gadgets %s)' % gad)
    check_font('-', e, 'Workbench')
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
        check(blits() > b0, 'the level change reached the window by a blit: ENV:AMARCHY_BLITS %d -> %d' % (b0, blits()))
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


def case_groups(geom):
    """Window groups (plan 2026-10-25-amarchy-window-groups.md, I3): the Screens and Workbench groups' rows draw
    their icons. The rig is OS 3.2, so the WORKBENCH port exists and both groups show. For each group: walk to its
    row in the root level (Down, Return), then the first and the second row's icon cells of that level, grabbed
    from the screen, hold at least 4 pixels unlike the row background; Esc steps back."""
    e = expected(geom, '-', wb=True)
    s, w = open_menu()
    if not check(bool(w), 'Super+Space opened a window titled "%s"' % TITLE):
        return
    time.sleep(2)
    for g in ('screens', 'workbench'):
        k = e.get('rows', {}).get(g)
        if not check(k is not None, 'the root level lists the %s group (rows %s)' % (g, e.get('rows'))):
            continue
        for _ in range(k):
            ami.key(DOWN)
            time.sleep(0.5)
        ami.key(RETURN)
        time.sleep(2)
        s, w = where(TITLE)
        if not check(bool(w), 'Return on %s kept the window' % g):
            continue
        ix, iy, cw, ch, iy2 = e[g]['cell']
        for row, y in (('first', iy), ('second', iy2)):
            n, bg = cell_pixels(s, box_of(s, w), ix, y, cw, ch)
            check(n >= 4, '%s group, %s row: icon cell %dx%d holds %d pixels unlike the row background %s' % (g, row, cw, ch, n, bg))
        ami.key(ESC)
        time.sleep(2)
    close_menu()


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
            check_font(name, e, 'public screen')
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


def case_run_tmux():
    """Apps > Tmux (run:tmux) from a menu with no TERM in ENV: leaves a running tmux and an UP-Term window.
    The leaf program has no shell above it, so TERM is missing unless run_in_uterm sets it; tmux then exits
    at once with "open terminal failed: not a terminal". Sentinel: the tmux server answers `tmux ls`."""
    c.run('tmux kill-server', 20)
    c.run('Delete >NIL: ENV:TERM')
    s, w = open_menu()
    check(bool(w), 'run_tmux: menu opens')
    ami.key(RETURN)                       # Apps
    time.sleep(2)
    for _ in range(2):                    # Tmux is the third Apps row
        ami.key(0x4D)
        time.sleep(.5)
    ami.key(RETURN)
    time.sleep(6)
    rc = c.run('tmux ls', 20)[0]
    check(rc == 0, 'run_tmux: tmux server runs after Apps > Tmux (tmux ls rc %s)' % rc)
    check(any(w['title'] == 'UP-Term' for s in screens() for w in s['windows']), 'run_tmux: an UP-Term window exists')
    c.run('tmux kill-server', 20)


def case_run_editor():
    """Apps > Editor (run:nvim): nvim is on vsh's $PATH (UP-Term:nvim/bin), not on the AmigaDOS path, so the
    leaf started nothing. The leaf runs it through the UP-Term shell now. Sentinel: an nvim process exists
    (Status COM=nvim) and an UP-Term window is open."""
    s, w = open_menu()
    check(bool(w), 'run_editor: menu opens')
    ami.key(RETURN)                       # Apps
    time.sleep(2)
    for _ in range(3):                    # Editor is the fourth Apps row
        ami.key(0x4D)
        time.sleep(.5)
    ami.key(RETURN)
    time.sleep(8)
    out = c.run('Status', 20)[1]
    check(re.search(r'\bnvim\b', out) is not None, 'run_editor: an nvim process runs after Apps > Editor (Status: %s)' % out.replace('\n', ' | ')[:120])
    check(any(w['title'] == 'UP-Term' for s in screens() for w in s['windows']), 'run_editor: an UP-Term window exists')


def case_run_addons():
    """Install add-ons (run:upkg browse) from the menu opens the upkg browse window. It ran `upkg-browse`, a script
    the kit no longer ships, so the menu closed and nothing opened. The root rows come from ENV:AMARCHY_ROWS
    (gui.c sentinel_rows); the Install group is picked by Down presses to its row, Enter drills in, Install
    add-ons is its first row. Sentinel: an upkg process runs (Status) and a window titled for upkg exists."""
    c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
    s, w = open_menu()
    check(bool(w), 'run_addons: menu opens')
    rows = []
    for _ in range(40):
        rc, out = c.run('GetEnv AMARCHY_ROWS')
        rows = out.strip().split('|') if rc == 0 and out.strip() else []
        if rows:
            break
        time.sleep(.25)
    idx = [i for i, r in enumerate(rows) if r.startswith('Install')]
    check(bool(idx), 'run_addons: the root has an Install row (rows: %s)' % rows)
    if not idx:
        close_menu()
        return
    for _ in range(idx[0]):
        ami.key(0x4D)
        time.sleep(.4)
    ami.key(RETURN)                       # into Install
    time.sleep(2)
    ami.key(RETURN)                       # Install add-ons, the first row
    time.sleep(8)
    out = c.run('Status', 20)[1]
    wins = [(sc.get('title'), w['title']) for sc in screens() for w in sc['windows']]
    print('run_addons: Status: %s' % out.replace('\n', ' | ')[:200])
    print('run_addons: windows: %s' % wins)
    check(re.search(r'\bupkg\b', out) is not None, 'run_addons: an upkg process runs after Install > Install add-ons')
    check(any('upkg' in t.lower() or 'add-on' in t.lower() or 'install' in t.lower() for _, t in wins),
          'run_addons: an upkg browse window exists')


def rows_now(tries=40):
    """The labels of the shown level, from ENV:AMARCHY_ROWS (gui.c sentinel_rows, published on every full draw)."""
    for _ in range(tries):
        rc, out = c.run('GetEnv AMARCHY_ROWS')
        if rc == 0 and out.strip():
            return out.strip().split('|')
        time.sleep(.25)
    return []


def walk_to(label, rows):
    """Down to the first row starting with label and Return on it; False when the level has no such row."""
    idx = [i for i, r in enumerate(rows) if r.startswith(label)]
    if not idx:
        return False
    for _ in range(idx[0]):
        ami.key(DOWN)
        time.sleep(.4)
    c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
    ami.key(RETURN)
    time.sleep(2)
    return True


def open_pkg_search():
    """Open the menu, walk root > Packages > Search; returns the Packages level's rows, or None when a step failed."""
    c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
    s, w = open_menu()
    if not check(bool(w), 'pkg_search: menu opens'):
        return None
    root = rows_now()
    if not check(walk_to('Packages', root), 'pkg_search: the root has a Packages row (rows: %s)' % root):
        return None
    pkgs = rows_now()
    if not check(walk_to('Search', pkgs), 'pkg_search: the Packages level has a Search row (rows: %s)' % pkgs):
        return None
    return pkgs


def case_pkg_search():
    """Packages > Search keeps the menu open (gui_ask_value restores the list mode and clears stop). The prompt's
    own Return or Escape set stop and left the menu in input mode, so the outer loop ended and the window closed
    with no result shown. Return on a text with no match: the window stays and the level is the search node, whose
    only row is the note "No packages here". Escape at the prompt: the window stays on the Packages level."""
    if open_pkg_search() is not None:
        for k in (0x31, 0x31, 0x10, 0x10):   # zzqq: no package matches
            ami.key(k)
            time.sleep(.3)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
        ami.key(RETURN)
        time.sleep(3)
        check(bool(where(TITLE)[1]), 'pkg_search: Return on "zzqq" keeps the window')
        got = rows_now()
        # index_core.c pkg_rows_out: "No packages here" with a loaded list, the other note on a rig with no feed;
        # either one means the search node is shown, which is what this case proves
        check(got in (['No packages here'], ['No packages listed (is upkg installed?)']),
              'pkg_search: the search node shows its no-match note (rows: %s)' % got)
    close_menu()
    pkgs = open_pkg_search()
    if pkgs is not None:
        c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
        ami.key(ESC)                          # Escape at the prompt
        time.sleep(3)
        check(bool(where(TITLE)[1]), 'pkg_search: Escape at the Search prompt keeps the window')
        got = rows_now()
        check(got == pkgs, 'pkg_search: Escape at the prompt shows the Packages level again (rows: %s)' % got)
    close_menu()


def setup():
    """--setup: the rig runs the build's Amarchy from VTCX:Apps/bin with UP-Term: assigned to VTCX:Apps, no hand setup."""
    for l in c.run('Status', 20)[1].splitlines():     # the kit Amarchy (and any earlier copy) must not hold the port
        if 'UP-Term:bin/' in l or SETUP_BIN in l:
            c.run('Break %s' % l.split()[1].rstrip(':'), 20)
    time.sleep(2)
    for n in SETUP_NAMES:
        ami.put(os.path.join(REPO, 'build/amiga', n), '%s/%s' % (SETUP_BIN, n))
    c.run('Assign UP-Term: VTCX:Apps ADD', 20)        # always with a target: Assign UP-Term: alone deletes it
    for n in SETUP_NAMES:                             # md5sum is on the rig once UP-Term:bin resolves; absent: unverified
        with open(os.path.join(REPO, 'build/amiga', n), 'rb') as fh:
            want = hashlib.md5(fh.read()).hexdigest()
        out = c.run('%s/md5sum %s/%s' % (SETUP_BIN, SETUP_BIN, n), 30)[1]
        got = re.search(r'[0-9a-f]{32}', out)
        if got:
            check(got.group(0) == want, 'setup: %s/%s matches build/amiga/%s' % (SETUP_BIN, n, n))
        else:
            print('setup: md5 of %s not checked (no md5sum on the rig): %s' % (n, out.strip()[:60]), flush=True)
    c.run('Run >NIL: %s/Amarchy' % SETUP_BIN, 20)
    for _ in range(20):
        if amarchy_window_rig.port('STATUS', 5)[0] is not None:
            return check(True, 'setup: Amarchy runs from %s, port AMARCHY answers' % SETUP_BIN)
        time.sleep(1)
    return check(False, 'setup: port AMARCHY did not appear in 20 s')


def main():
    if '--help' in sys.argv or '-h' in sys.argv:
        print(__doc__)
        return 0
    if '--setup' in sys.argv:
        setup()
        if bad or len(sys.argv) == 2:
            return 1 if bad else 0
    stage_rig.report(stage_rig.stage_build(NAMES, build='--no-build' not in sys.argv))
    geom = build_geom()
    stage_fonts()
    if '--setup' not in sys.argv:      # --setup already started the Amarchy it set up
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        time.sleep(3)
    try:
        if '--groups' in sys.argv:      # on an OS 3.2 rig (the WORKBENCH port exists): only the group icons
            case_groups(geom)
        elif '--pkg-search' in sys.argv:  # only Packages > Search (needs upkg on the rig)
            case_pkg_search()
        else:
            case_default(geom)
            case_screen(geom, True)
            case_screen(geom, False)
            case_run_tmux()
            case_run_editor()
            case_run_addons()
            case_pkg_search()
    finally:
        c.run('Assign FONTS: VTC:%s REMOVE' % FONT_DRAWER)
    c.run('Delete >NIL: ENV:AMARCHY_DRAWS')
    print('FAIL' if bad else 'PASS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
