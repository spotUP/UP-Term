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
  * Packages > Install and Remove against a local feed (--pkg-install runs only this): the Browse rows equal the feed,
    Install runs the job view to rc 0 and the row reads Installed, Remove's confirm offers No first, No keeps the
    package, Yes removes it, and no console or terminal window opens
  * Packages > Upgrades against a local feed (--pkg-upgrades runs only this): a bumped feed makes the installed package
    upgradable, "Upgrade all" is the first row, the package row matches upkg list-upgradable, Upgrade runs to rc 0 and
    the row goes; AMARCHY_PKGS_LOADS is 1 after three opens and rises by one per job
  * Packages > Install of a package whose .ipk the feed does not serve (--pkg-fail runs only this): the job ends with a
    failing rc (13: a 404 download; the plan said 12), the row never reads Installed, no console or terminal window opens
  * Packages > Install of a name the menu lists but upkg no longer knows (--pkg-notfound runs only this): rc 12 and
    AMARCHY_JOB_WORD "Package not found"; --pkg-fail also asserts AMARCHY_JOB_WORD "Network error"
  * Packages > Browse rows carry [upkg] / [UHC] tags (--pkg-tags runs only this): a staged UHC catalogue entry reads [UHC],
    every feed package [upkg]
  * AMB3: with a second PUBLIC screen in front the menu and the leaf it opens (Apps, UP-Term) appear on it, and
    the menu is sized for that screen's font; with a screen that is NOT public in front they fall back to Workbench
Self-checking. Rig up (rig.py start --max, UPTERM_RIG=<n>), handler installed. Default keymap and menu style
are assumed (no ENV:up-term/keymap override). Run: python3 tools/rig/amarchy_gui_rig.py [--setup] [--no-build]
--setup first prepares the rig by itself (kills the kit Amarchy, puts the build to VTCX:Apps/bin, assigns UP-Term:
and starts that Amarchy); alone (or with only --no-build) it runs only that. --help prints this text."""
import hashlib, os, re, shlex, shutil, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c, stage_rig, amarchy_window_rig, upkg_rig

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
    for _ in range(6):                  # one Escape climbs one level: a section row is four levels deep
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


PKG_CONF = 'VTCX:Apps/etc/upkg.conf'    # shadows the kit's UP-Term:etc/upkg.conf (UP-Term: = VTCX:Apps, then the kit)
PKG_PICK = ('up-pi', 'up-git', 'up-bitchx', 'up-perl')   # smallest first; not up-claude: it carries install questions (rc 15, job_core.c)
CONSOLE_WORDS = ('con:', 'console', 'terminal', 'shell')


def feed_dir():
    """build/ipk of this checkout, else of the main checkout the worktree belongs to (`make dist-ipk` needs the ports tree)."""
    for base in (REPO, os.path.dirname(os.path.dirname(os.path.dirname(REPO)))):
        d = os.path.join(base, 'build/ipk')
        if os.path.exists(os.path.join(d, 'Packages')):
            return d
    raise SystemExit('no build/ipk/Packages: run make dist-ipk')


def all_titles():
    return [w['title'] for s in screens() for w in s['windows']]


def job_done(secs=240):
    """Waits for ENV:AMARCHY_JOB_DONE (job_dos.c: rc + 1); returns rc, or None on timeout."""
    end = time.time() + secs
    while time.time() < end:
        rc, out = c.run('GetEnv AMARCHY_JOB_DONE')
        if rc == 0 and out.strip().isdigit():
            return int(out.strip()) - 1
        time.sleep(2)
    return None


def pkg_group(tag='pkg_install'):
    """Menu open, root > Packages; the Packages level's rows (None on a failed step)."""
    c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
    s, w = open_menu()
    if not check(bool(w), '%s: menu opens' % tag):
        return None
    root = rows_now()
    if not check(walk_to('Packages', root), '%s: the root has a Packages row (rows: %s)' % (tag, root)):
        return None
    return rows_now()


def installed_names():
    out = upkg_rig.upkg('pg8-listinst', 'list-installed', 300, conf=False)[1]
    return set(re.findall(r'^(\S+) - ', out, re.M))


def local_feed(fd, packages=None):
    """Release serving fd (with `packages` as its Packages text when given) and the shadow conf written to PKG_CONF;
    the caller deletes PKG_CONF and closes the Release."""
    rel = upkg_rig.Release()
    rel.set(fd, packages=packages)
    c.run('MakeDir VTCX:Apps/etc', 20)
    tmpconf = os.path.join(tempfile.mkdtemp(prefix='pg8conf'), 'upkg.conf')
    open(tmpconf, 'w').write(upkg_rig.conf_for(open(os.path.join(REPO, 'dist/upkg.conf')).read(), rel.url))
    ami.put(tmpconf, PKG_CONF)
    return rel


def feed_names(text):
    return [p['Package'] for p in upkg_rig.parse_packages(text)]


def env_num(name):
    rc, out = c.run('GetEnv %s' % name)
    return int(out.strip()) if rc == 0 and out.strip().lstrip('-').isdigit() else None


def case_pkg_upgrades():
    """Packages > Upgrades (plan PG.8). Setup without the menu: `upkg install up-pi` against the normal local feed
    (the Install path is case_pkg_install's); then the feed serves up-pi at a higher version. Loads: ix_add_pkg_rows
    (index_core.c) bumps pk_loads on every load; the list loads when not loaded or stale, and a finished job marks it
    stale, so the count runs over the warm server's life: 1 for any number of opens, +1 per job."""
    fd = feed_dir()
    pkg, table_id, high = 'up-pi', 'pi', '1.1'
    rel = local_feed(fd)
    try:
        if not check(pkg in [p['Package'] for p in upkg_rig.parse_packages(open(os.path.join(fd, 'Packages')).read())],
                     'pkg_upgrades: %s is in the feed' % pkg):
            return
        if pkg in installed_names():
            upkg_rig.upkg('pg8u-pre', 'remove %s' % pkg, 300, conf=False)
        rc = upkg_rig.upkg('pg8u-update', 'update', 300, conf=False)[0]
        rc2 = upkg_rig.upkg('pg8u-inst', 'install %s' % pkg, 600, conf=False)[0]
        check(rc == 0 and rc2 == 0 and pkg in installed_names(), 'pkg_upgrades: %s installed through upkg (rc %s/%s)' % (pkg, rc, rc2))
        # fresh Amarchy: loads count from 0
        setup()
        # The warm menu server keeps the package cache across opens: the first open loads (1); the sentinel is
        # published only by a load, so after deleting it two more opens (menu closed between) must not bring it back.
        for i in range(3):
            if i:
                c.run('Delete >NIL: ENV:AMARCHY_PKGS_LOADS QUIET')
            pkgs = pkg_group()
            if pkgs is None:
                return
            time.sleep(1)
            loads = env_num('AMARCHY_PKGS_LOADS')
            if i == 0:
                check(loads == 1, 'pkg_upgrades: AMARCHY_PKGS_LOADS is 1 after the first open (got %s)' % loads)
            else:
                check(loads is None, 'pkg_upgrades: open %d did not reload the cache (loads sentinel absent, got %s)' % (i + 1, loads))
            close_menu()
        rel.set(rel.bumped_feed(table_id, high, only=True))
        pkgs = pkg_group()
        if pkgs is None:
            return
        c.run('Delete >NIL: ENV:AMARCHY_JOB_DONE QUIET')
        if not check(walk_to('Refresh', pkgs), 'pkg_upgrades: Refresh row (rows: %s)' % pkgs):
            return
        rc = job_done()
        check(rc == 0, 'pkg_upgrades: Refresh job rc 0 (rc %s)' % rc)
        ami.key(RETURN)                      # accept: the job marked the list stale, the level reloads in the same menu
        time.sleep(3)
        loads = env_num('AMARCHY_PKGS_LOADS')
        check(loads == 2, 'pkg_upgrades: AMARCHY_PKGS_LOADS is 2 after one job: counted over the server life (got %s)' % loads)
        close_menu()
        pkgs = pkg_group()
        if pkgs is None:
            return
        out = upkg_rig.upkg('pg8u-lu', 'list-upgradable', 300, conf=False)[1]
        print('pkg_upgrades: list-upgradable: %s' % out.strip()[:300], flush=True)
        want = set(re.findall(r'^(\S+) - ', out, re.M))
        if not check(walk_to('Upgrades', pkgs), 'pkg_upgrades: Upgrades row (rows: %s)' % pkgs):
            return
        rows = rows_now()
        print('pkg_upgrades: upgrade rows: %s' % rows, flush=True)
        check(bool(rows) and rows[0].startswith('Upgrade all'), 'pkg_upgrades: Upgrade all is the first row (rows: %s)' % rows)
        got = {n for n in want if any(n in r for r in rows[1:])}
        check(got == want and any(pkg in r for r in rows[1:]), 'pkg_upgrades: package rows match upkg list-upgradable %s (rows: %s)' % (sorted(want), rows))
        # upkg list --tsv (checked on the rig 2026-10-27) emits two upgradable lines for one package, the available 1.1 and
        # the installed 1.0, so Upgrades shows two up-pi rows; reported, not asserted: it is upkg's list, not the menu's
        hit = [r for r in rows if pkg in r and high in r.split()]
        if not check(len(hit) == 1, 'pkg_upgrades: one up-pi row reads the higher version %s (rows: %s)' % (high, rows)):
            return
        print('pkg_upgrades: up-pi rows shown: %d (list-upgradable lists one)' % len([r for r in rows if pkg in r]), flush=True)
        for _ in range(rows.index(hit[0])):
            ami.key(DOWN)
            time.sleep(.4)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS ENV:AMARCHY_JOB_DONE QUIET')
        ami.key(RETURN)
        time.sleep(2)
        acts = rows_now()
        print('pkg_upgrades: action rows: %s' % acts, flush=True)
        if not check(walk_to('Upgrade', acts), 'pkg_upgrades: the row offers Upgrade (rows: %s)' % acts):
            return
        rc = job_done()
        check(rc == 0, 'pkg_upgrades: Upgrade job rc 0 (rc %s)' % rc)
        ami.key(RETURN)
        time.sleep(3)
        out = upkg_rig.upkg('pg8u-li', 'list-installed', 300, conf=False)[1]
        check(re.search(r'^%s - %s\b' % (re.escape(pkg), re.escape(high)), out, re.M) is not None,
              'pkg_upgrades: list-installed shows %s at %s (%s)' % (pkg, high, [l for l in out.splitlines() if pkg in l]))
        check(not [r for r in rows_now() if pkg in r], 'pkg_upgrades: Upgrades no longer lists %s (rows: %s)' % (pkg, rows_now()))
        close_menu()
        c.run('Delete >NIL: ENV:AMARCHY_PKGS_LOADS QUIET')
        pkg_group()
        time.sleep(1)
        loads = env_num('AMARCHY_PKGS_LOADS')
        print('pkg_upgrades: loads after the upgrade job and a reopen: %s (plan: no number)' % loads, flush=True)
    finally:
        close_menu()
        if pkg in installed_names():
            upkg_rig.upkg('pg8u-cleanup', 'remove %s' % pkg, 300, conf=False)
        c.run('Delete >NIL: %s QUIET' % PKG_CONF)
        rel.close()


def refresh_to_browse(names, tag):
    """Menu open > Packages > Refresh (job rc 0, accepted), menu reopened > Packages > Browse, into the one "other"
    section when the feed has no Section; the Browse rows (None on a failed step)."""
    pkgs = pkg_group(tag)
    if pkgs is None:
        return None
    c.run('Delete >NIL: ENV:AMARCHY_JOB_DONE QUIET')
    if not check(walk_to('Refresh', pkgs), '%s: the Packages level has a Refresh row (rows: %s)' % (tag, pkgs)):
        return None
    rc = job_done()
    check(rc == 0, '%s: Packages > Refresh job rc 0 (rc %s)' % (tag, rc))
    ami.key(RETURN)                      # accept the job view
    time.sleep(3)
    close_menu()
    pkgs = pkg_group(tag)
    if pkgs is None or not check(walk_to('Browse', pkgs), '%s: the Packages level has a Browse row (rows: %s)' % (tag, pkgs)):
        return None
    rows = rows_now()
    print('%s: browse rows: %s' % (tag, rows), flush=True)
    if len(rows) == 1 and not any(n in rows[0] for n in names):   # pkgs_core.c sec_of: rows group by section; a feed with no Section is one "other" group
        check(walk_to(rows[0], rows), '%s: Browse shows the one section %s; into it' % (tag, rows))
        rows = rows_now()
        print('%s: section rows: %s' % (tag, rows), flush=True)
    return rows


def case_pkg_install():
    """Packages > Install and Remove on a row of a local feed (plan 2026-10-27-amarchy-packages-group.md PG.8).
    Feed: upkg_rig.Release serves build/ipk on 127.0.0.1; the rig's upkg reads UP-Term:etc/upkg.conf, which
    VTCX:Apps/etc/upkg.conf (written here, deleted in the finally) shadows because --setup assigns UP-Term: to
    VTCX:Apps first. Sentinels: ENV:AMARCHY_JOB_DONE = rc + 1 (job_dos.c) and the row text. The confirm check
    reads rows_now()[0] of the Remove level: pkgs_core.c puts No first, so a Yes-first confirm fails it."""
    fd = feed_dir()
    names = feed_names(open(os.path.join(fd, 'Packages')).read())
    rel = local_feed(fd)
    pick = None
    try:
        have = installed_names()
        print('pkg_install: installed before: %s' % sorted(have), flush=True)
        pick = next((p for p in PKG_PICK if p in names and p not in have), None)
        if not check(pick is not None, 'pkg_install: a small feed package is not installed (feed %s)' % names):
            return
        base_titles = set(all_titles())
        rows = refresh_to_browse(names, 'pkg_install')
        if rows is None:
            return
        check(len(rows) == len(names), 'pkg_install: Browse has %d rows = the feed size (got %d)' % (len(names), len(rows)))
        hit = [r for r in rows if pick in r]
        if not check(len(hit) == 1, 'pkg_install: Browse has the row for %s (%s)' % (pick, hit)):
            return
        check('Installed' not in hit[0], 'pkg_install: the row does not read Installed before (%s)' % hit[0])
        for _ in range(rows.index(hit[0])):
            ami.key(DOWN)
            time.sleep(.4)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS ENV:AMARCHY_JOB_DONE QUIET')
        ami.key(RETURN)
        time.sleep(2)
        acts = rows_now()
        print('pkg_install: action rows: %s' % acts, flush=True)
        if not check(walk_to('Install', acts), 'pkg_install: the row offers Install (rows: %s)' % acts):
            return
        rc = job_done()
        check(rc == 0, 'pkg_install: Install job ends with rc 0 (rc %s)' % rc)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
        ami.key(RETURN)                      # accept
        time.sleep(3)
        rows = rows_now()
        hit = [r for r in rows if pick in r]
        check(len(hit) == 1 and 'Installed' in hit[0], 'pkg_install: the row now reads Installed (%s)' % hit)
        check(pick in installed_names(), 'pkg_install: upkg list-installed lists %s' % pick)
        rows = rows_now()
        hit = [r for r in rows if pick in r]
        if not check(len(hit) == 1, 'pkg_install: the %s row is shown for Remove (%s)' % (pick, hit)):
            return
        for _ in range(rows.index(hit[0])):  # the reload after a job puts the bar on the first row
            ami.key(DOWN)
            time.sleep(.4)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
        ami.key(RETURN)
        time.sleep(2)
        acts = rows_now()
        if not check(walk_to('Remove', acts), 'pkg_install: the row offers Remove (rows: %s)' % acts):
            return
        conf = rows_now()
        print('pkg_install: confirm rows: %s' % conf, flush=True)
        # pkgs_core.c confirm rows are "Remove <name>: No" then ": Yes"; a Yes-first order fails this check
        check(len(conf) == 2 and conf[0].endswith(': No') and conf[1].endswith(': Yes'),
              'pkg_install: the Remove confirm lists No first (rows: %s)' % conf)
        walk_to(conf[0], conf)               # Return on No: no job, the package stays
        time.sleep(3)
        check(pick in installed_names(), 'pkg_install: No keeps %s installed' % pick)
        after = rows_now()
        print('pkg_install: level after No: %s' % after, flush=True)
        if after != conf:                    # back at the row's own level: ask again
            if not check(walk_to('Remove', after), 'pkg_install: Remove is offered again after No (rows: %s)' % after):
                return
            conf = rows_now()
        c.run('Delete >NIL: ENV:AMARCHY_JOB_DONE QUIET')
        if not check(walk_to(conf[1], conf), 'pkg_install: Return on the Yes row (rows: %s)' % conf):
            return
        rc = job_done()
        check(rc == 0, 'pkg_install: Remove job ends with rc 0 (rc %s)' % rc)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
        ami.key(RETURN)
        time.sleep(3)
        hit = [r for r in rows_now() if pick in r]
        check(len(hit) == 1 and 'Installed' not in hit[0], 'pkg_install: the row no longer reads Installed (%s)' % hit)
        check(pick not in installed_names(), 'pkg_install: upkg list-installed no longer lists %s' % pick)
        new = [t for t in all_titles() if t not in base_titles and any(k in t.lower() for k in CONSOLE_WORDS)]
        check(not new, 'pkg_install: no console or terminal window opened (new: %s)' % new)
    finally:
        close_menu()
        if pick and pick in installed_names():
            upkg_rig.upkg('pg8-cleanup', 'remove %s' % pick, 300, conf=False)
        c.run('Delete >NIL: %s QUIET' % PKG_CONF)
        rel.close()


PKG_FAIL = 'up-zzfail'     # a Packages stanza whose .ipk the local feed does not serve


def ghost_stanza(name):
    """A Packages stanza whose .ipk the local feed does not serve."""
    return ('Package: %s\nVersion: 1.0\nArchitecture: m68k-amigaos\nFilename: %s_1.0_m68k-amigaos.ipk\n'
            'Size: 1234\nSHA256sum: %s\nDescription: rig failing package\n' % (name, name, '0' * 64))


def job_word():
    rc, out = c.run('GetEnv AMARCHY_JOB_WORD')
    return out.strip() if rc == 0 else None


PKG_UHC = 'pg8uhc'          # the one UHC catalogue entry the rig stages (upkg reads SYS:UHC/Packages when ENV:UHC/UHCBIN is unset)


def uhc_catalogue():
    """Stages a UHC catalogue drawer (SYS:UHC/Packages, one script PKG_UHC and its .desc) so `upkg list --tsv` yields a
    src uhc row; the rig has no UHC add-on. Returns the cleanup callable (removes SYS:UHC; the rig had none)."""
    d = tempfile.mkdtemp(prefix='pg8uhc')
    open(os.path.join(d, PKG_UHC), 'w').write('Set PKGNAME "%s"\nSet PKGVERSION "1.0"\nSet UHCRC 0\n' % PKG_UHC)
    open(os.path.join(d, PKG_UHC + '.desc'), 'w').write('PG8 rig UHC package\n')
    c.run('MakeDir SYS:UHC', 20)
    c.run('MakeDir SYS:UHC/Packages', 20)
    for f in (PKG_UHC, PKG_UHC + '.desc'):
        ami.put(os.path.join(d, f), 'SYS:UHC/Packages/' + f)
    return lambda: c.run('Delete >NIL: SYS:UHC ALL QUIET', 30)


def tag_problems(rows, feed, uhc, windowed=False):
    """Browse rows (a row reads "[upkg] name ..." or "[UHC] name ..."; pkgs_core.c pk_label): what is wrong with their tags,
    [] when every feed package row carries [upkg] and every uhc package row [UHC]. windowed: the style shows a window of the
    list (text menu, 8 rows), so a name that is not shown is skipped; the shown rows must still carry both tags."""
    bad = []
    for names, tag in ((feed, '[upkg] '), ((uhc,), '[UHC] ')):
        for n in names:
            hit = [r for r in rows if re.search(r'(^|\s)%s\b' % re.escape(n), r)]
            if windowed and not hit:
                continue
            if len(hit) != 1 or not hit[0].lstrip().startswith(tag + n):
                bad.append('%s: %s' % (n, hit))
    if windowed and not (any(r.lstrip().startswith('[upkg] ') for r in rows) and any(r.lstrip().startswith('[UHC] ') for r in rows)):
        bad.append('the shown rows lack a [upkg] row or a [UHC] row: %s' % rows)
    return bad


def case_pkg_tags():
    """Packages > Browse rows carry [upkg] (an ipk of the feed) and [UHC] (a UHC catalogue entry) tags (plan PG.8)."""
    fd = feed_dir()
    feed = feed_names(open(os.path.join(fd, 'Packages')).read())
    drop = uhc_catalogue()
    rel = local_feed(fd)
    try:
        rows = refresh_to_browse(feed + [PKG_UHC], 'pkg_tags')
        if rows is None:
            return
        bad = tag_problems(rows, feed, PKG_UHC)
        check(not bad and len(rows) == len(feed) + 1, 'pkg_tags: %d feed rows read [upkg], %s reads [UHC] (rows %d; problems %s)' % (len(feed), PKG_UHC, len(rows), bad))
    finally:
        close_menu()
        c.run('Delete >NIL: %s QUIET' % PKG_CONF)
        rel.close()
        drop()


def case_pkg_fail():
    """Packages > Install of a package whose .ipk 404s (plan PG.8: a failing job shows Failed with its word).
    The footer word comes from job_core.c rc_words (a 404 on the .ipk is rc 13, "Network error"; rc 12, "Package not found", is a name missing from the index; case_pkg_notfound); the rig asserts the rc (ENV:AMARCHY_JOB_DONE = rc + 1), the word (ENV:AMARCHY_JOB_WORD), that the row never reads Installed and that no console
    window opened; the rc-to-word table is tests/test_job.c."""
    fd = feed_dir()
    text = open(os.path.join(fd, 'Packages')).read()
    extra = ghost_stanza(PKG_FAIL)
    names = feed_names(text + '\n' + extra)
    rel = local_feed(fd, packages=text.rstrip('\n') + '\n\n' + extra)
    try:
        base_titles = set(all_titles())
        rows = refresh_to_browse(names, 'pkg_fail')
        if rows is None:
            return
        hit = [r for r in rows if PKG_FAIL in r]
        if not check(len(hit) == 1, 'pkg_fail: Browse has the row for %s (%s)' % (PKG_FAIL, hit)):
            return
        check('Installed' not in hit[0], 'pkg_fail: the row does not read Installed before (%s)' % hit[0])
        for _ in range(rows.index(hit[0])):
            ami.key(DOWN)
            time.sleep(.4)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS ENV:AMARCHY_JOB_DONE ENV:AMARCHY_JOB_WORD QUIET')
        ami.key(RETURN)
        time.sleep(2)
        acts = rows_now()
        if not check(walk_to('Install', acts), 'pkg_fail: the row offers Install (rows: %s)' % acts):
            return
        rc = job_done()
        check(rc not in (None, 0, 5), 'pkg_fail: the Install job ends with a failing rc (rc %s)' % rc)
        check(job_word() == 'Network error', 'pkg_fail: AMARCHY_JOB_WORD is "Network error" (got %r)' % job_word())
        check(rc == 13, 'pkg_fail: the rc is 13, Network error: the .ipk download 404s (plan said 12, which is a package missing from the index; rc %s)' % rc)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
        ami.key(RETURN)                      # accept
        time.sleep(3)
        hit = [r for r in rows_now() if PKG_FAIL in r]
        check(len(hit) == 1 and 'Installed' not in hit[0], 'pkg_fail: the row does not read Installed after (%s)' % hit)
        check(PKG_FAIL not in installed_names(), 'pkg_fail: upkg list-installed does not list %s' % PKG_FAIL)
        new = [t for t in all_titles() if t not in base_titles and any(k in t.lower() for k in CONSOLE_WORDS)]
        check(not new, 'pkg_fail: no console or terminal window opened (new: %s)' % new)
    finally:
        close_menu()
        c.run('Delete >NIL: %s QUIET' % PKG_CONF)
        rel.close()


PKG_GONE = 'up-zzgone'     # listed by the feed at Refresh, gone from it (and from upkg's index) before the Install


def case_pkg_notfound():
    """Packages > Install of a package the menu lists but upkg no longer knows (plan PG.8: rc 12, "Package not found").
    The feed lists PKG_GONE; Refresh loads it into upkg's index and the menu cache. Then the feed drops it and
    `upkg update` (outside the menu, so the menu cache is not marked stale) refreshes upkg's index only. Install from the
    menu row runs `upkg install` of a name upkg lacks: rc 12, and AMARCHY_JOB_WORD is the footer word."""
    fd = feed_dir()
    text = open(os.path.join(fd, 'Packages')).read()
    names = feed_names(text + '\n' + ghost_stanza(PKG_GONE))
    rel = local_feed(fd, packages=text.rstrip('\n') + '\n\n' + ghost_stanza(PKG_GONE))
    try:
        base_titles = set(all_titles())
        rows = refresh_to_browse(names, 'pkg_notfound')
        if rows is None:
            return
        hit = [r for r in rows if PKG_GONE in r]
        if not check(len(hit) == 1, 'pkg_notfound: Browse has the row for %s (%s)' % (PKG_GONE, hit)):
            return
        close_menu()
        rel.set(fd, packages=text)           # the feed no longer lists it
        rc = upkg_rig.upkg('pg8nf-update', 'update', 300, conf=False)[0]
        if not check(rc == 0, 'pkg_notfound: upkg update against the reduced feed rc 0 (rc %s)' % rc):
            return
        rows = pkg_group('pkg_notfound')
        if rows is None or not check(walk_to('Browse', rows), 'pkg_notfound: Browse row (rows: %s)' % rows):
            return
        rows = rows_now()
        if len(rows) == 1 and not any(n in rows[0] for n in names):
            walk_to(rows[0], rows)
            rows = rows_now()
        hit = [r for r in rows if PKG_GONE in r]
        if not check(len(hit) == 1, 'pkg_notfound: the menu cache still lists %s (rows: %s)' % (PKG_GONE, rows)):
            return
        for _ in range(rows.index(hit[0])):
            ami.key(DOWN)
            time.sleep(.4)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS ENV:AMARCHY_JOB_DONE ENV:AMARCHY_JOB_WORD QUIET')
        ami.key(RETURN)
        time.sleep(2)
        acts = rows_now()
        if not check(walk_to('Install', acts), 'pkg_notfound: the row offers Install (rows: %s)' % acts):
            return
        rc = job_done()
        word = job_word()
        check(rc == 12, 'pkg_notfound: the Install job ends with rc 12 (rc %s)' % rc)
        check(word == 'Package not found', 'pkg_notfound: AMARCHY_JOB_WORD is "Package not found" (got %r)' % word)
        c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
        ami.key(RETURN)                      # accept
        time.sleep(3)
        hit = [r for r in rows_now() if PKG_GONE in r]
        check(not any('Installed' in r for r in hit), 'pkg_notfound: the row never reads Installed (%s)' % hit)
        check(PKG_GONE not in installed_names(), 'pkg_notfound: upkg list-installed does not list %s' % PKG_GONE)
        new = [t for t in all_titles() if t not in base_titles and any(k in t.lower() for k in CONSOLE_WORDS)]
        check(not new, 'pkg_notfound: no console or terminal window opened (new: %s)' % new)
    finally:
        close_menu()
        c.run('Delete >NIL: %s QUIET' % PKG_CONF)
        rel.close()


def setup():
    """--setup: the rig runs the build's Amarchy from VTCX:Apps/bin with UP-Term: assigned to VTCX:Apps, no hand setup."""
    for l in c.run('Status', 20)[1].splitlines():     # the kit Amarchy (and any earlier copy) must not hold the port
        if 'UP-Term:bin/' in l or SETUP_BIN in l:
            c.run('Break %s' % l.split()[1].rstrip(':'), 20)
    time.sleep(2)
    # Lock("UP-Term:bin") on a multi-directory assign returns only the first match, VTCX:Apps/bin; pl_dup_path puts
    # that one lock on the children's path, so upkg and the rest of the kit must be in it. Kit first, build over it.
    c.run('Copy Rushhours:Tools/UP-Term/bin %s ALL QUIET' % SETUP_BIN, 120)
    for n in SETUP_NAMES:
        ami.put(os.path.join(REPO, 'build/amiga', n), '%s/%s' % (SETUP_BIN, n))
    # VTCX:Apps first so the fresh build shadows the kit's copies in UP-Term:bin;
    # the kit goes after it so upkg and the rest of the kit still resolve.
    # Always with a target: Assign UP-Term: alone deletes the assign.
    c.run('Assign UP-Term: VTCX:Apps', 20)
    c.run('Assign UP-Term: Rushhours:Tools/UP-Term ADD', 20)
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
        if bad or not set(sys.argv[1:]) - {'--setup', '--no-build'}:   # setup builds nothing: --no-build adds no case
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
        elif '--pkg-tags' in sys.argv:  # only the [upkg] / [UHC] tags of the Browse rows
            case_pkg_tags()
        elif '--pkg-fail' in sys.argv:  # only Packages > Install of a package the feed cannot deliver
            case_pkg_fail()
        elif '--pkg-notfound' in sys.argv:  # only Packages > Install of a name upkg no longer knows (rc 12)
            case_pkg_notfound()
        elif '--pkg-install' in sys.argv:  # only Packages > Install and Remove against a local feed
            case_pkg_install()
        elif '--pkg-upgrades' in sys.argv:  # only Packages > Upgrades against a bumped local feed
            case_pkg_upgrades()
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
            case_pkg_install()
            case_pkg_upgrades()
            case_pkg_tags()
            case_pkg_fail()
            case_pkg_notfound()
    finally:
        c.run('Assign FONTS: VTC:%s REMOVE' % FONT_DRAWER)
    c.run('Delete >NIL: ENV:AMARCHY_DRAWS')
    print('FAIL' if bad else 'PASS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
