#!/usr/bin/env python3
"""amarchy_menu_rig.py -- the Amarchy quick menu and the handler request port
(plan 2026-10-22-amarchy-commodity.md, Phase D; request port: handler/upreq.h, keymap_act.h upreq_*).

Table driven and resumable, like amarchy_rig.py: a verdict per row in <RIG>/amarchy/menu_<row>.verdict,
--only <row>, --failed, --fresh. Rows share state (the commodity running, an UP-Term window to act on).

Sentinel for "the request reached the handler": every request the handler runs bumps STATE_KEYMAP, so
`UPTerm state` km-hits moves by exactly 1 per delivered leaf (a key binding moves it the same way, one
code path). The chord for /theme is NOT needed: the request port works with it unbound.
Needs the rig up (rig.py start --max), Amarchy and amarchy-menu in UP-Term:bin (rig.py install puts them there), the handler of this branch
installed. NOT RUN when written: the rows stay open until it has run.
"""
import argparse, pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig, tabstate

OUT = pathlib.Path(paths.RIG) / 'amarchy'
LAMIGA = 0x40                       # IEQUALIFIER_LCOMMAND
RAW = {'down': 0x4D, 't': 0x14, 'h': 0x25, 'k': 0x27, 'space': 0x40, 'esc': 0x45, 'return': 0x44}
KEYMAP = pathlib.Path(__file__).resolve().parents[2] / 'dist' / 'keymap'
TIMING = pathlib.Path(paths.RIG) / 'amarchy' / 'menu-timing.txt'


def km_hits():
    return tabstate.counters().get('km-hits', 0)


def screen_text():
    return ami.screen_text()


def bind_rows():
    n, sect = 0, ''
    for l in KEYMAP.read_text().splitlines():
        l = l.strip()
        if l.startswith('['):
            sect = l
        elif sect == '[bind]' and l and not l.startswith(';') and '=' in l:
            # "super+1..9" is one line of the file and nine rows (dist/keymap header, handler keymap parser)
            m = re.match(r'\S*?(\d)\.\.(\d)\s*=', l)
            n += int(m.group(2)) - int(m.group(1)) + 1 if m else 1
    return n


def open_terminal():
    # a titled UP-Term window the handler owns: `UPTerm state` and the request port need one
    tabstate.open_host('menuhost')


def ensure_terminal():
    # the cases that open the menu with Left Amiga + Alt + Space type into the UP-Term window under it; --only
    # on a fresh rig skips rows_over_terminal, which opened it, so these cases open it themselves (once)
    import wbrun_rig
    if not any('menuhost' in (w.get('title') or '') for w in wbrun_rig.windows()):
        open_terminal()


def r_start():
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    return True, 'commodity started'


def root_menu_rows():
    # open the quick menu with Super+Space. The default menu is the drawn one (amarchy-gui, window "Amarchy"):
    # its text is pixels, so the rows come from the sentinel ENV:AMARCHY_ROWS (gui.c sentinel_rows: the labels
    # of the rows of each list draw, joined by '|'). Deleted first, read once the window is up, Esc leaves.
    import amarchy_gui_rig as g
    c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
    s, w = g.open_menu()
    rows = ''
    for _ in range(40):
        rc, out = c.run('GetEnv AMARCHY_ROWS')
        rows = out.strip() if rc == 0 else ''
        if w and rows:
            break
        time.sleep(0.25)
    ami.key(RAW['esc'])
    time.sleep(1)
    return bool(w), rows.split('|') if rows else []


def r_rows_no_terminal():
    # TERM-ROWS-ONLY-OVER-TERMINAL: no UP-Term window exists yet, the menu is the only one. The rows
    # whose action needs a terminal (Style, Window: tab:, menuid:, slash:) are hidden, Apps stays.
    up, rows = root_menu_rows()
    ok = up and 'Apps' in rows and 'Style' not in rows and 'Window' not in rows
    return ok, 'no UP-Term window: drawn root rows %s (menu up %s)' % (rows, up)


def r_rows_over_terminal():
    # the same menu over an UP-Term window lists Style and Window again
    open_terminal()
    up, rows = root_menu_rows()
    ok = up and 'Apps' in rows and 'Style' in rows and 'Window' in rows
    return ok, 'over an UP-Term window: drawn root rows %s (menu up %s)' % (rows, up)


def r_open_timing():
    t0 = time.time()
    ami.key(RAW['space'], LAMIGA)
    while time.time() - t0 < 10:
        if 'Filter' in screen_text():
            break
        time.sleep(0.1)
    dt = time.time() - t0
    OUT.mkdir(parents=True, exist_ok=True)
    TIMING.write_text('menu open (Super+Space to "Filter:" on screen): %.2f s\n' % dt)
    return 'Filter' in screen_text(), 'menu open %.2f s -> %s' % (dt, TIMING)


def into_style():
    # The root lists Apps, Style, Window (submenus); Theme is the first leaf of Style: Down, Enter descends.
    # (The old stale-hotkey ordering rule is gone: upterm-ports ebf5d1c makes ui_hotkey() describe the last run only.)
    for k in 'down', 'return':
        ami.key(RAW[k])
        time.sleep(0.6)


def r_type_th():
    into_style()
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.4)
    s = screen_text()
    return 'Filter: th' in s and 'Theme' in s, 'filter "th" leaves Theme in the list'


def r_esc_up():
    ami.key(RAW['esc'])           # clears the filter
    time.sleep(0.5)
    ami.key(RAW['esc'])           # in Style: back up to the root
    time.sleep(0.5)
    up = 'Style' in screen_text()
    ami.key(RAW['esc'])           # at the root: leaves the menu
    time.sleep(0.8)
    return up and 'Filter' not in screen_text(), 'Esc clears the filter, Esc goes up to the root (Style listed: %s), Esc at the root leaves' % up


def r_enter_slash():
    # TERM-ROWS-ONLY-OVER-TERMINAL: the default menu is the drawn one (amarchy-gui, a plain Intuition window
    # outside the focus record) and exactly one UP-Term window is open (rows_over_terminal's host): its slash:
    # row must reach that terminal (upmru_target self 0).
    # reopen, Style, Enter on Theme: the request reaches the handler, which runs /theme through the keymap runner.
    # /theme opens a file requester ("UP-Term theme") and the handler sits in it until it closes, so the
    # effect is the requester on screen; the counter is read after its Cancel gadget (id 2) closed it, as keymap_rig.py does.
    before = km_hits()
    ami.key(RAW['space'], LAMIGA)
    time.sleep(1.5)
    into_style()
    ami.key(RAW['return'])             # Theme is the first row
    time.sleep(2)
    with ami._conn() as conn:
        shown = '"UP-Term theme"' in ami._uitree(conn).decode('latin-1')
        if shown:
            ami._click_gadget(conn, 'UP-Term theme', '2')
    time.sleep(1)
    ami.click_px(100, 100)             # the requester had the focus; the vsh host takes it back
    time.sleep(1)
    after = km_hits()
    return shown and after == before + 1, 'theme requester shown: %s; handler km-hits %+d (exactly 1 expected)' % (shown, after - before)


def viewer_lines(s):
    return set(l.strip(' |') for l in s.splitlines() if re.match(r'\|\s*(Super|Alt)\+', l))


def r_viewer_rows():
    # the viewer is a text box (amarchy/menu.c viewer, K_TEXTBOX in a 640x300 window): one page of rows at a time.
    # Scroll with Down, a few lines per step, and collect every distinct row line seen until a step adds none.
    ami.key(RAW['k'], LAMIGA)
    time.sleep(1.5)
    rows = viewer_lines(screen_text())
    for _ in range(40):
        for _ in range(6):
            ami.key(RAW['down'])
            time.sleep(0.15)
        time.sleep(0.4)
        new = viewer_lines(screen_text()) - rows
        if not new:
            break
        rows |= new
    ami.key(RAW['esc'])
    time.sleep(0.8)
    n = bind_rows()
    return n > 0 and len(rows) == n, 'dist/keymap has %d [bind] rows; viewer printed %d distinct rows while scrolling' % (n, len(rows))


def menu_rows(s):
    # the list rows of the menu box, labels only, in screen order (the frame and Filter line left out)
    # the OSD menu has no frame ("  Apps    >"); a framed box ("|  Apps  |") is read the same way
    return [re.split(r'\s{2,}', l.strip(' |'))[0] for l in s.splitlines() if re.match(r'\|?  \S', l) and 'Filter' not in l]


def theme_requester(before):
    # the /theme requester is the proof the leaf ran; Cancel it (gadget 2) and give the host window the focus back
    time.sleep(2)
    with ami._conn() as conn:
        shown = '"UP-Term theme"' in ami._uitree(conn).decode('latin-1')
        if shown:
            ami._click_gadget(conn, 'UP-Term theme', '2')
    time.sleep(1)
    ami.click_px(100, 100)
    time.sleep(1)
    return shown, km_hits() - before


def r_drill_redraw():
    # Owner bug AM-DRILL-REDRAW (A1200): Style drawn over the root, then "th" left "   me" and a stale Font row.
    # Cause: ncurses tparm memo keyed by the %i-bumped cup values (upterm-ports 7600cda), so rows landed one
    # line off. The cells must hold exactly the submenu's rows after the drill and after the filter.
    ami.key(RAW['space'], LAMIGA)
    time.sleep(1.5)
    into_style()
    style = menu_rows(screen_text())
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.5)
    th = menu_rows(screen_text())
    for _ in range(3):
        ami.key(RAW['esc'])
        time.sleep(0.5)
    ok = style == ['Theme', 'Font', 'Background'] and th == ['Theme']
    return ok, 'Style rows %s; after "th" %s' % (style, th)


def r_enter_th():
    # Owner bug AM-ENTER-H: Enter after the filter "th" added another "h" (stale ui_hotkey, upterm-ports ebf5d1c).
    # Enter must run Theme: the requester opens and km-hits moves by 1.
    before = km_hits()
    ami.key(RAW['space'], LAMIGA)
    time.sleep(1.5)
    into_style()
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.5)
    ami.key(RAW['return'])
    shown, d = theme_requester(before)
    return shown and d == 1, 'filter "th", Enter: theme requester shown: %s; km-hits %+d (1 expected)' % (shown, d)


def r_lamiga_minus_sv():
    # Owner bug AM-MINUS-EXIT: Swedish keymap, Left Amiga + "-" (raw 0x3A there) made Amarchy exit on the A1200.
    # With the keymap set to "s", the chord must reach the handler (km-hits +1) and the commodity must stay up.
    import amarchy_rig
    c.run('SetKeyboard s')
    try:
        before = km_hits()
        ami.key(0x3A, LAMIGA)
        time.sleep(2)
        d, up = km_hits() - before, amarchy_rig.port_up()
    finally:
        c.run('SetKeyboard usa')
    return d == 1 and up, 'Swedish Left Amiga+minus: km-hits %+d (1 expected), Amarchy port up: %s' % (d, up)


# ---- AM-IDX X9: the app index through the real entry points (plan 2026-10-23-amarchy-app-index.md) ----
# Fake apps in drawers on the rig hard disk (the boot volume, named System: on OS 3.1 rigs and by its label on 3.2 ones, so SYS:; the labels start A0 so the filter "a0" narrows the
# Apps level to them: a rig holds many apps and the 24x7 menu shows a level's first rows only), tests/amiga/wbfake (writes RAM:x9.<file>.started, waits 2 s, writes
# .ended), icons made by tools/mkicon.py. amarchy-index scans, the text menu (Left Amiga + Alt + Space) lists them,
# Enter launches through lf_open_index. On a rig Directory Opus replaces Workbench, so OpenWorkbenchObject fails
# and every launch here takes the AMARCHY.launch port fallback; the OpenWorkbenchObject path needs the A1200.
X9_DIR, X9_SKIP = 'SYS:X9Test', 'SYS:X9Skip'      # x9_begin respells X9_SKIP with the boot volume's name, see there
X9_CONF = 'ENV:up-term/up-term'
X9_CACHE = 'ENVARC:Amarchy/apps'
ALT = 0x10
ROOT = pathlib.Path(__file__).resolve().parents[2]


def x9_icon(remote, *args):
    import subprocess, tempfile
    with tempfile.TemporaryDirectory() as d:
        f = pathlib.Path(d) / 'i.info'
        subprocess.run([sys.executable, '-I', str(ROOT / 'tools/mkicon.py'), str(f), '--plain'] + list(args), check=True)
        ami.put(str(f), remote)


def x9_files(extra=()):
    fake = str(ROOT / 'build/amiga/wbfake')
    c.run('MakeDir >NIL: %s %s' % (X9_DIR, X9_SKIP))
    for d, name in ((X9_DIR, 'A0Fake'), (X9_DIR, 'A0Shadow'), (X9_SKIP, 'A0Skipped')) + tuple(extra):
        ami.put(fake, '%s/%s' % (d, name))
        x9_icon('%s/%s.info' % (d, name), '--wbtool')
    x9_icon(X9_DIR + '/A0Game.info', '--tool', 'WHDLoad')       # WBPROJECT, default tool WHDLoad


def x9_open_menu():
    ami.click_px(100, 100)                     # the host terminal has the focus
    time.sleep(0.5)
    ami.key(RAW['space'], LAMIGA | ALT)        # othermenu:root, the text menu
    for _ in range(30):
        if 'Filter' in screen_text():
            return True
        time.sleep(0.5)
    return False


def x9_type(text):
    with ami._conn() as conn:
        ami._type(conn, text.encode())
    time.sleep(0.8)


def x9_level(label):
    # from the root, Down to the group `label`, Enter; the screen text of that level. A root without the
    # group (the X10 revert check takes Games away) stays at the root, so the level's checks fail, not the case
    rows = menu_rows(screen_text())
    if label not in rows:
        return screen_text()
    for _ in range(rows.index(label)):
        ami.key(RAW['down'])
        time.sleep(0.3)
    ami.key(RAW['return'])
    time.sleep(1)
    return screen_text()


def x9_leave():
    for _ in range(4):
        ami.key(RAW['esc'])
        time.sleep(0.5)


def x9_wait_file(path, secs=15):
    end = time.time() + secs
    while time.time() < end:
        if ami.read_file(path) is not None:
            return True
        time.sleep(1)
    return False


def x9_upterm_target():
    """where UP-Term: points now (the first directory of its `Assign LIST` line), None when unassigned"""
    for line in c.run('Assign LIST')[1].splitlines():
        f = line.split(None, 1)
        if len(f) == 2 and f[0].upper() == 'UP-TERM':
            return f[1].strip()
    return None


def x9_begin():
    """Saves the files a case rewrites, points UP-Term: at the staged build (Rescan apps runs amarchy-index from
    UP-Term:bin; on a rig with a kit installed UP-Term: points at the kit, not at the build stage_rig puts in
    VTC:Apps/bin) and clears the case's files. Returns what x9_end gives back."""
    global X9_SKIP
    # amarchy-index-skip is a literal path prefix against the scanned path, which starts with the volume's name
    # (System: on one rig, Empty-12MB-FFS: on the 3.2 rig), so the skip entry spells the volume, not SYS:
    m = re.search(r'^SYS\s+(\S.*:)\s*$', c.run('Assign LIST')[1], re.M)
    X9_SKIP = (m.group(1) if m else 'SYS:') + 'X9Skip'
    saved = {p: ami.read_file(p) for p in (X9_CONF, 'ENV:up-term/keymap.user', X9_CACHE)}
    upterm = x9_upterm_target()
    c.run('Assign UP-Term: VTC:%s' % paths.UPTERM_DRAWER)
    c.run('Delete >NIL: RAM:x9.#? QUIET')
    c.run('MakeDir >NIL: ENV:up-term')
    return saved, upterm


def x9_end(saved, upterm):
    import amarchy_rig
    x9_leave()
    time.sleep(3)
    c.run('Delete >NIL: %s %s ALL QUIET' % (X9_DIR, X9_SKIP))
    c.run('Delete >NIL: %s.info %s.info QUIET' % (X9_DIR, X9_SKIP))
    for p, b in saved.items():
        ami.restore_file(p, b)
    c.run('Delete >NIL: RAM:x9.#? QUIET')
    c.run('Assign UP-Term: "%s"' % upterm if upterm else 'Assign UP-Term: REMOVE')
    amarchy_rig.arexx('RELOAD')


def r_app_icons():
    """AM-IDX-ICON I3: the drawn menu (amarchy-gui, the default hotkey Left Amiga + Space) shows an index row's own
    .info image in the row's icon cell. The fake WBTOOL app A0Fake has its icon; A0NoIcon is indexed and then loses
    its .info, so its row must show the generic Apps glyph, which is also what the root level's Apps row draws.
    Sentinel: ENV:AMARCHY_ICON_LOADS (icon_gfx.c, .info images loaded) is deleted first and is above 0 after the
    menu drew A0Fake. Needs icon.library V44 (OS 3.2: `rig.py start --os 32 --max`)."""
    import amarchy_rig, amarchy_gui_rig as g
    res = []

    def ck(name, ok, detail=''):
        res.append(ok)
        print('  %s %s%s' % ('[OK]' if ok else '[FAIL]', name, (': ' + detail) if detail else ''), flush=True)
        return ok

    def loads():
        rc, out = c.run('GetEnv AMARCHY_ICON_LOADS')
        m = re.match(r'\s*(\d+)\s*$', out) if rc == 0 else None
        return int(m.group(1)) if m else 0

    def cell(e, level, row=0):
        s, w = g.where(g.TITLE)
        if not w:
            return None
        ix, iy, cw, ch, iy2 = e[level]['cell']
        iy = iy2 if row else iy
        return g.cell_grid(s, g.box_of(s, w), ix, iy, cw, ch)[0]

    def inked(rows, bg):
        return sum(1 for r in rows for x in range(len(r) // 3) if r[x * 3:x * 3 + 3] != bg)

    if not amarchy_rig.port_up():
        r_start()
    stage_rig.report(stage_rig.stage_build(('scrprobe', 'wbfake'), build=True))
    geom = g.build_geom()
    e = g.expected(geom, '-')
    saved, upterm = x9_begin()
    try:
        x9_files([(X9_DIR, 'A0NoIcon')])
        c.run('Echo >%s "[profile default]"' % X9_CONF)
        c.run('Echo >>%s "amarchy-index-skip = %s"' % (X9_CONF, X9_SKIP))
        amarchy_rig.arexx('RELOAD')
        c.run('Delete >NIL: %s QUIET' % X9_CACHE)
        c.run('%s/amarchy-index' % paths.UPTERM_BIN, 120)
        cache = (ami.read_file(X9_CACHE) or b'').decode('latin-1')
        ck('scanner indexed A0Fake and A0NoIcon', re.search(r'^app\tA0Fake\t', cache, re.M) is not None
           and re.search(r'^app\tA0NoIcon\t', cache, re.M) is not None)
        c.run('Delete >NIL: %s/A0NoIcon.info QUIET' % X9_DIR)       # indexed, but its image is gone
        c.run('Delete >NIL: ENV:AMARCHY_ICON_LOADS ENV:AMARCHY_DRAWS QUIET')
        ck('sentinel ENV:AMARCHY_ICON_LOADS deleted before the run', loads() == 0)
        ami.click_px(100, 100)
        time.sleep(0.5)
        s, w = g.open_menu()
        if not ck('drawn menu opened', bool(w)):
            return False, 'the drawn menu did not open'
        time.sleep(2)
        ck('amarchy-gui is the running process', g.process_running('amarchy-gui'))
        root_glyph = cell(e, 'root')             # row 0 of the root is Apps, selected: the generic glyph
        bg = bytes(g.cell_grid(s, g.box_of(s, w), e['root']['cell'][0] - 0, e['root']['cell'][1], e['root']['cell'][2], e['root']['cell'][3])[1])
        ck('root level Apps row draws its glyph', inked(root_glyph, bg) >= 4, '%d pixels' % inked(root_glyph, bg))
        ami.key(g.RETURN)
        time.sleep(2)
        x9_type('a0')                           # the filter leaves the two staged rows: A0Fake first, A0NoIcon second
        time.sleep(1.5)
        icon = cell(e, 'apps')
        n_icon = inked(icon, bg) if icon else 0
        ck('sentinel: ENV:AMARCHY_ICON_LOADS = %d after A0Fake drew (the icon loader ran)' % loads(), loads() > 0)
        ck('A0Fake row: icon cell holds pixels', n_icon >= 4, '%d pixels' % n_icon)
        ck('A0Fake row: its own icon, not the generic glyph', icon is not None and icon != root_glyph)
        ami.key(RAW['down'])                    # row 1 selected: the same selection colour as the root glyph reference
        time.sleep(1.0)
        fall = cell(e, 'apps', 1)
        ck('A0NoIcon row shows the generic glyph (the cell equals the root Apps row)', fall is not None and fall == root_glyph,
           '%d pixels' % (inked(fall, bg) if fall else 0))
        ck('the two icon rows differ', fall is not None and icon is not None and fall != icon)
    finally:
        g.close_menu()
        x9_end(saved, upterm)
    return all(res), '%d of %d checks passed' % (sum(res), len(res))


def r_app_index():
    import amarchy_rig, wbrun_rig
    res = []

    def ck(name, ok, detail=''):
        res.append(ok)
        print('  %s %s%s' % ('[OK]' if ok else '[FAIL]', name, (': ' + detail) if detail else ''), flush=True)
        return ok

    if not amarchy_rig.port_up():
        r_start()
    ensure_terminal()
    saved, upterm = x9_begin()
    try:
        x9_files()
        c.run('Echo >%s "[profile default]"' % X9_CONF)
        c.run('Echo >>%s "amarchy-index-skip = %s"' % (X9_CONF, X9_SKIP))
        # step 6: a [menu] row with the label of an index row
        c.run('Echo >ENV:up-term/keymap.user "[menu]"')
        c.run('Echo >>ENV:up-term/keymap.user "apps.a0shadow = A0Shadow | run:C:Echo >RAM:x9.menurow hit"')
        amarchy_rig.arexx('RELOAD')
        c.run('Delete >NIL: %s QUIET' % X9_CACHE)
        rc, out = c.run('%s/amarchy-index' % paths.UPTERM_BIN, 120)
        cache = (ami.read_file(X9_CACHE) or b'').decode('latin-1')
        ck('1/2 scanner wrote a row (kind app) for the fake tool', re.search(r'^app\tA0Fake\t[^\t]*:X9Test/A0Fake$', cache, re.M) is not None,
           'rc %d, cache %d bytes' % (rc, len(cache)))
        ck('4 the WHDLoad project is a game row', re.search(r'^game\tA0Game\t', cache, re.M) is not None)
        ck('5 the skip-listed drawer is not indexed', 'A0Skipped' not in cache and 'A0Fake' in cache)
        # step 3: menu, Apps level, launch
        ck('3 menu opens', x9_open_menu())
        root = screen_text()
        apps = x9_level('Apps')
        # the menu is 24x7 cells: the level shows its first rows only, so the filter narrows it to the fake rows
        x9_type('a0')
        apps = screen_text()
        ami.key(RAW['esc'])                    # clears the filter
        time.sleep(0.5)
        ck('3 Apps level lists the row', 'A0Fake' in apps, ' '.join(menu_rows(apps))[:100])
        ck('4 Apps level does not list the game', 'A0Game' not in apps)
        ck('5 Apps level does not list the skipped app', 'A0Skipped' not in apps)
        ck('6 one A0Shadow row at Apps (the [menu] row)', len(re.findall('A0Shadow', apps)) == 1)
        ami.key(RAW['esc'])
        time.sleep(0.8)
        games = x9_level('Games')
        ck('4 Games level lists the project', 'A0Game' in games, ' '.join(menu_rows(games))[:100])
        ami.key(RAW['esc'])
        time.sleep(0.8)
        x9_level('Apps')
        x9_type('a0shadow')
        ami.key(RAW['return'])
        ok6 = x9_wait_file('RAM:x9.menurow', 10)
        time.sleep(3)
        ck('6 launching A0Shadow ran the [menu] row, not the tool', ok6 and ami.read_file('RAM:x9.A0Shadow.started') is None)
        x9_leave()
        c.run('Delete >NIL: RAM:x9.#? QUIET')
        x9_open_menu()
        x9_level('Apps')
        x9_type('a0fake')
        ami.key(RAW['return'])
        started = x9_wait_file('RAM:x9.A0Fake.started', 20)
        ck('3 launch: the tool ran (sentinel written)', started)
        time.sleep(1)
        ck('3 launcher exited (amarchy-menu not running)', 'amarchy-menu' not in c.run('Status')[1])
        ended = x9_wait_file('RAM:x9.A0Fake.ended', 10)
        time.sleep(2)
        ck('3 the app quit and the commodity survived', ended and amarchy_rig.port_up())
        ck('3 no requester', not ami.requesters(), '; '.join(ami.describe_requester(r) for r in ami.requesters()))
        # step 7: Rescan apps rewrites the cache and opens no window
        x9_files([(X9_DIR, 'A0Late')])
        old = cache
        x9_open_menu()
        x9_level('Apps')
        win0 = wbrun_rig.window_ids()
        x9_type('rescan')
        ami.key(RAW['return'])
        time.sleep(1)
        new_win = [w for w in wbrun_rig.windows() if wbrun_rig.win_id(w) not in win0 and 'amarchy' not in w['title'].lower()]
        ck('7 Rescan apps opens no window', not new_win, str([w['title'] for w in new_win]))
        end = time.time() + 40
        cache2 = old
        while time.time() < end and 'A0Late' not in cache2:
            time.sleep(2)
            cache2 = (ami.read_file(X9_CACHE) or b'').decode('latin-1')
        ck('7 Rescan apps rewrote the cache (A0Late is in)', 'A0Late' in cache2 and cache2 != old)
        x9_leave()
        # step 8: Apps > Clock (index row of SYS:Utilities/Clock), through the AMARCHY.launch fallback
        ck('8 Clock is indexed', re.search(r'^app\tClock\t', cache2, re.M) is not None)
        x9_open_menu()
        x9_level('Apps')
        x9_type('clock')
        w0 = wbrun_rig.window_ids()
        ami.key(RAW['return'])
        w = wbrun_rig.wait_window(w0, lambda w: w.get('task') == 'Clock', 20)
        ck('8 Apps > Clock opens the Clock window (AMARCHY.launch fallback; OpenWorkbenchObject fails under Directory Opus)', w is not None)
        if not w:
            # failure output: what was open and running when no Clock window showed up
            print('  [INFO] windows: %s' % [(x.get('task'), x.get('title')) for x in wbrun_rig.windows()])
            print('  [INFO] Status: %s' % c.run('Status')[1].replace('\n', ' | ')[:1500])
        if w:
            quit_ = [m for m in w.get('menus', []) if ami._m().ui_norm(m['path']).endswith('quit')]
            if quit_:
                with ami._conn() as conn:
                    ami._m()
                    import struct
                    conn.send(ami._m().UIACT, struct.pack('>III', 1, int(w['addr'], 16), quit_[0]['code']))
                    conn.expect_ok()
            ck('8 Clock quits', bool(quit_) and wbrun_rig.wait_gone(w))
    finally:
        x9_end(saved, upterm)
    return all(res), '%d of %d checks passed' % (sum(res), len(res))


ROWS = [
    ('start',        r_start),
    ('rows_no_terminal', r_rows_no_terminal),
    ('rows_over_terminal', r_rows_over_terminal),
    ('open_timing',  r_open_timing),
    ('type_th',      r_type_th),
    ('esc_up',       r_esc_up),
    ('enter_slash',  r_enter_slash),
    ('viewer_rows',  r_viewer_rows),
    ('drill_redraw', r_drill_redraw),
    ('enter_th',     r_enter_th),
    ('lamiga_minus_sv', r_lamiga_minus_sv),
    ('app_index',    r_app_index),
    ('app_icons',    r_app_icons),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true', help='stage what build/amiga holds, do not run make')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    # the rows run VTC:Apps/bin/Amarchy and amarchy-menu: those files must be this build
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    passed = total = 0
    for name, fn in ROWS:
        f = OUT / ('menu_' + name + '.verdict')
        if a.only and a.only != name:
            continue
        if f.exists() and not a.fresh and not a.only:
            old = f.read_text()
            if old.startswith('ok') or not a.failed:
                total += 1
                passed += old.startswith('ok')
                continue
        ok, detail = fn()
        f.write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
    print('amarchy_menu_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
