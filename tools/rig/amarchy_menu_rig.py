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
import argparse, json, pathlib, re, subprocess, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig, tabstate

OUT = pathlib.Path(paths.RIG) / 'amarchy'
LAMIGA = 0x40                       # IEQUALIFIER_LCOMMAND
LSHIFT = 0x01                       # IEQUALIFIER_LSHIFT
ALT = 0x10                          # IEQUALIFIER_LALT
# Super+Space opens the drawn menu (pixels, no screen text); the rows below that read the menu through
# screen_text() open the text menu (amarchy-menu in the UP-Term window): over a terminal Super+Space opens it
# (menu-in-terminal = text, the default)
TEXT_MENU = LAMIGA
RAW = {'down': 0x4D, 't': 0x14, 'h': 0x25, 'k': 0x27, 'space': 0x40, 'esc': 0x45, 'return': 0x44, 'o': 0x18, 'd': 0x22}
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
    # the rig's shell path is C: only; a real Workbench has SYS:Rexxc in it, and the commodity's rexx: rows run `RX`
    # with the path it inherits, so the rig gives it the same
    c.run('Path SYS:Rexxc ADD\nRun >NIL: %s/Amarchy' % paths.UPTERM_BIN)
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
    # whose action needs a terminal (tab:, menuid:, slash:: the Style group) are hidden; Apps stays, and so does Window,
    # whose win: rows (cycle, move, size ...) act on any window and are shown with no UP-Term window (9ad02f94).
    up, rows = root_menu_rows()
    ok = up and 'Apps' in rows and 'Style' not in rows and 'Window' in rows
    return ok, 'no UP-Term window: drawn root rows %s (menu up %s)' % (rows, up)


def r_rows_over_terminal():
    # the same menu over an UP-Term window lists Style and Window again
    open_terminal()
    up, rows = root_menu_rows()
    ok = up and 'Apps' in rows and 'Style' in rows and 'Window' in rows
    return ok, 'over an UP-Term window: drawn root rows %s (menu up %s)' % (rows, up)


def r_open_timing():
    t0 = time.time()
    ami.key(RAW['space'], TEXT_MENU)
    while time.time() - t0 < 10:
        if 'Filter' in screen_text():
            break
        time.sleep(0.1)
    dt = time.time() - t0
    OUT.mkdir(parents=True, exist_ok=True)
    TIMING.write_text('menu open (Super+Space to "Filter:" on screen): %.2f s\n' % dt)
    return 'Filter' in screen_text(), 'menu open %.2f s -> %s' % (dt, TIMING)


def into_style():
    # Theme is the first leaf of Style. The root's order moves (Dopus and Settings come before Style), so the
    # level is found by its label, not by a fixed number of Down presses.
    # (The old stale-hotkey ordering rule is gone: upterm-ports ebf5d1c makes ui_hotkey() describe the last run only.)
    x9_level('Style')


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
    ami.key(RAW['space'], TEXT_MENU)
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
    ami.key(RAW['k'], TEXT_MENU)       # Super+K over the terminal: the text viewer
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
    ami.key(RAW['space'], TEXT_MENU)
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
    ami.key(RAW['space'], TEXT_MENU)
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
    # Symbol key names (minus = the key that prints "-") are resolved when a table loads: Amarchy's at RELOAD, the
    # handler's when the first UP-Term window opens (g_km_users), so the keymap is set to Swedish before a fresh
    # window opens and Amarchy reloads. Both counter reads happen on the USA keymap: the rig types "VTC:UPTerm
    # state" through wasabi with US raw codes, which the Swedish keymap turns into other characters.
    import amarchy_rig, vshwin
    vshwin.close_vsh('menuhost')
    time.sleep(1)
    try:
        c.run('SetKeyboard s')
        amarchy_rig.arexx('RELOAD')
        time.sleep(2)
        open_terminal()
        c.run('SetKeyboard usa')
        before = km_hits()
        c.run('SetKeyboard s')
        ami.key(0x3A, LAMIGA)
        time.sleep(2)
    finally:
        c.run('SetKeyboard usa')
    d, up = km_hits() - before, amarchy_rig.port_up()
    # the later rows get the USA table back: the Swedish window goes, the next one loads the table again
    amarchy_rig.arexx('RELOAD')
    vshwin.close_vsh('menuhost')
    time.sleep(1)
    open_terminal()
    return d == 1 and up, 'Swedish Left Amiga+minus: km-hits %+d (1 expected), Amarchy port up: %s' % (d, up)


# ---- AM-IDX X9: the app index through the real entry points (plan 2026-10-23-amarchy-app-index.md) ----
# Fake apps in drawers on the rig hard disk (the boot volume, named System: on OS 3.1 rigs and by its label on 3.2 ones, so SYS:; the labels start A0 so the filter "a0" narrows the
# Apps level to them: a rig holds many apps and the 24x7 menu shows a level's first rows only), tests/amiga/wbfake (writes RAM:x9.<file>.started, waits 2 s, writes
# .ended), icons made by tools/mkicon.py. amarchy-index scans, the text menu (Left Amiga + Alt + Space) lists them,
# Enter launches through lf_open_index. On a rig Directory Opus replaces Workbench, so OpenWorkbenchObject fails
# and every launch here takes the AMARCHY.launch port fallback; the OpenWorkbenchObject path needs the A1200.
X9_DIR, X9_SKIP = 'SYS:X9Test', 'SYS:X9Skip'      # x9_begin respells X9_SKIP with the boot volume's name, see there
X9_CONF = 'ENV:up-term/up-term'
X9_AMCONF = 'ENV:Amarchy/prefs'     # Amarchy's own store since 5270f4ba; it is read before the up-term file
AM_KEYS_C = pathlib.Path(__file__).resolve().parents[2] / 'amarchy' / 'amconf_keys.c'
X9_CACHE = 'ENVARC:Amarchy/apps'
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
    ami.key(RAW['space'], LAMIGA)              # over the terminal: the text menu
    for _ in range(30):
        if 'Filter' in screen_text():
            return True
        time.sleep(0.5)
    return False


def x9_type(text):
    # one KEY frame per character: a whole string in one frame loses all but the first two or so characters
    # on rig 4 (fast "Open UP-Term here" left the filter at "Op"); one key per 100 ms delivers every one
    with ami._conn() as conn:
        for ch in text:
            ami._type(conn, ch.encode())
            time.sleep(0.1)
    time.sleep(0.8)


# the scan status row of amarchy/index_core.c ix_watch_row, the first entry of a level while it is shown
SCAN_STATUS = re.compile(r'Scan(ning\.\.\. | done: | did not start| stopped | could not save)')


def x9_level(label):
    # from the root, Down to the group `label`, Enter; the screen text of that level. A root without the
    # group (the X10 revert check takes Games away) stays at the root, so the level's checks fail, not the case
    rows = menu_rows(screen_text())
    if label not in rows:
        return screen_text()
    # a level opens with the cursor on its first item, below the scan status row (menu.c default_item)
    first = 1 if rows and SCAN_STATUS.match(rows[0]) else 0
    for _ in range(rows.index(label) - first):
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
    saved = {p: ami.read_file(p) for p in (X9_CONF, X9_AMCONF, 'ENV:up-term/keymap.user', X9_CACHE)}
    upterm = x9_upterm_target()
    c.run('Assign UP-Term: VTC:%s' % paths.UPTERM_DRAWER)
    c.run('Delete >NIL: RAM:x9.#? QUIET')
    c.run('MakeDir >NIL: ENV:up-term ENV:Amarchy')
    return saved, upterm


def am_key_names():
    """The keys Amarchy's own store owns: the names in the product table am_keys[] (amarchy/amconf_keys.c)."""
    return set(re.findall(r'^\s*\{\s*"([a-z-]+)"\s*,', AM_KEYS_C.read_text(), re.M))


def x9_conf(*lines):
    """Write a case's settings, each 'key = value' line to the store that owns the key: Amarchy keys to
    ENV:Amarchy/prefs, every other key to ENV:up-term/up-term. Both files are rewritten, so a case starts from
    defaults; Amarchy reloads on the notify of ENV:Amarchy/prefs (the RELOAD the cases send follows)."""
    import amarchy_rig
    owned = am_key_names()
    am = [l for l in lines if l.split('=')[0].strip() in owned]
    up = [l for l in lines if l not in am]
    for path, rows in ((X9_AMCONF, am), (X9_CONF, up)):
        c.run('Echo >%s "[profile default]"' % path)
        for l in rows:
            c.run('Echo >>%s "%s"' % (path, l))


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


def icon_library_major():
    """Major version of the icon.library in memory (0 when unreadable). icon_gfx.c ig_open opens it at V44, and
    without it the index rows keep the generic glyph and ENV:AMARCHY_ICON_LOADS stays 0 (the default rig is
    KS 3.1 with icon.library 40)."""
    rc, out = c.run('Version icon.library')
    m = re.search(r'icon\.library\s+(\d+)\.', out) if rc == 0 else None
    return int(m.group(1)) if m else 0


def r_app_icons():
    """AM-IDX-ICON I3: the drawn menu (amarchy-gui, the default hotkey Left Amiga + Space) shows an index row's own
    .info image in the row's icon cell. The fake WBTOOL app A0Fake has its icon; A0NoIcon is indexed and then loses
    its .info, so its row must show the generic Apps glyph, which is also what the root level's Apps row draws.
    Sentinel: ENV:AMARCHY_ICON_LOADS (icon_gfx.c, .info images loaded) is deleted first and is above 0 after the
    menu drew A0Fake. Needs icon.library V44 (OS 3.2: `rig.py start --os 32 --max`) and the upconf key
    amarchy-app-icons = amiga (a12e12f2: generic glyphs by default), which the row writes itself."""
    import amarchy_rig, amarchy_gui_rig as g
    ensure_terminal()
    res = []
    if icon_library_major() < 44:
        return False, ('icon.library is below V44 on this rig, so amarchy-gui draws only the generic glyph '
                       '(icon_gfx.c ig_open); start it with `rig.py start --os 32`')

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
        # since a12e12f2 the drawn menu loads .info images only with the app-icons key; without it every row
        # draws a generic glyph and AMARCHY_ICON_LOADS stays 0
        x9_conf('amarchy-index-skip = %s' % X9_SKIP, 'amarchy-app-icons = amiga')
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


def r_minimal():
    """MENU-MINIMAL: menu-style = minimal makes the drawn menu (amarchy-gui, Left Amiga + Space) plain text rows with
    two pens (the fg and bg keys), no border, no icon column and no chevron art. Sentinels (gui.c, beside
    AMARCHY_ICON_LOADS): ENV:AMARCHY_MODE (2 = minimal), ENV:AMARCHY_PENS (distinct pens obtained: 2) and
    ENV:AMARCHY_ICON_LOADS (0: no .info image is loaded even with amarchy-app-icons = amiga), all deleted first.
    The second step reopens with menu-style = graphical: MODE 0 and more than 2 pens. The pixel check counts the
    distinct colours inside the window."""
    import amarchy_rig, amarchy_gui_rig as g
    res = []

    def ck(name, ok, detail=''):
        res.append(ok)
        print('  %s %s%s' % ('[OK]' if ok else '[FAIL]', name, (': ' + detail) if detail else ''), flush=True)
        return ok

    def colours():
        s, w = g.where(g.TITLE)
        if not w:
            return -1
        left, top, ww, hh = g.box_of(s, w)
        gw, gh, rows = ami.grab((left + 6, top + 6, ww - 12, hh - 12))
        return len({bytes(r[x * 3:x * 3 + 3]) for r in rows for x in range(len(r) // 3)})

    def opened(style):
        x9_conf('menu-style = %s' % style, 'fg = E0E0A0', 'bg = 102040', 'amarchy-app-icons = amiga')
        amarchy_rig.arexx('RELOAD')
        c.run('Delete >NIL: ENV:AMARCHY_MODE ENV:AMARCHY_PENS ENV:AMARCHY_ICON_LOADS ENV:AMARCHY_DRAWS QUIET')
        ami.click_px(100, 100)
        time.sleep(0.5)
        s, w = g.open_menu()
        time.sleep(2)
        return bool(w)

    if not amarchy_rig.port_up():
        r_start()
    ensure_terminal()
    saved, upterm = x9_begin()
    try:
        c.run('Echo >ENV:up-term/keymap.user "[menu]"')
        ck('minimal: drawn menu opened', opened('minimal'))
        ck('minimal: sentinel AMARCHY_MODE = %d' % g.env_count('AMARCHY_MODE'), g.env_count('AMARCHY_MODE') == 2)
        ck('minimal: sentinel AMARCHY_PENS = %d (two colours)' % g.env_count('AMARCHY_PENS'), g.env_count('AMARCHY_PENS') == 2)
        ck('minimal: AMARCHY_ICON_LOADS = %d (no .info image)' % g.env_count('AMARCHY_ICON_LOADS'),
           g.env_count('AMARCHY_ICON_LOADS') == 0)
        ck('minimal: AMARCHY_DRAWS = %d (the menu drew)' % g.draws(), g.draws() >= 1)
        n = colours()
        ck('minimal: %d distinct colours inside the window (no border art, no glyphs)' % n, 0 < n <= 2)
        g.close_menu()
        ck('graphical: drawn menu opened', opened('graphical'))
        ck('graphical: AMARCHY_MODE = %d' % g.env_count('AMARCHY_MODE'), g.env_count('AMARCHY_MODE') == 0)
        ck('graphical: AMARCHY_PENS = %d (more than two)' % g.env_count('AMARCHY_PENS'), g.env_count('AMARCHY_PENS') > 2)
        n2 = colours()
        ck('graphical: %d distinct colours (more than minimal)' % n2, n2 > n)
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
        x9_conf('amarchy-index-skip = %s' % X9_SKIP)
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
        x9_level('Games')
        x9_type('a0')                           # a filter searches every category (the level lists drawers first)
        games = screen_text()
        ck('4 Games level lists the project', 'A0Game' in games, ' '.join(menu_rows(games))[:100])
        ami.key(RAW['esc'])
        time.sleep(0.8)
        ami.key(RAW['esc'])                     # the first Esc cleared the filter, this one leaves Games
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
        # step 7 (RESCAN-FEEDBACK): Rescan apps keeps the menu open and shows the scan
        x9_files([(X9_DIR, 'A0Late')])
        old = cache
        x9_open_menu()
        x9_level('Apps')
        x9_type('rescan')
        ami.key(RAW['return'])
        seen, text = False, ''
        for _ in range(40):                     # the status row shows the scan at once; poll fast, it can be short
            text = screen_text()
            if 'Scanning...' in text:
                seen = True
                break
            if 'Scan done' in text:
                break
            time.sleep(0.25)
        ck('7 the menu window stays open after Rescan apps', 'Filter' in text, ' '.join(menu_rows(text))[:100])
        ck('7 a row starting "Scanning..." appears', seen, ' '.join(menu_rows(text))[:100])
        m, end = None, time.time() + 90
        while time.time() < end:
            m = re.search(r'Scan done: (\d+) apps', screen_text())
            if m:
                break
            time.sleep(1)
        ck('7 a row "Scan done: N apps" appears', m is not None)
        where_out = c.run('%s/amarchy-index --where' % paths.UPTERM_BIN, 30)[1]
        mw = re.search(r'(\d+) apps, (\d+) games', where_out)
        ck('7 N equals the count amarchy-index --where prints', bool(m and mw and m.group(1) == mw.group(1)),
           'row %s, --where %s' % (m.group(1) if m else None, mw.group(1) if mw else None))
        cache2 = (ami.read_file(X9_CACHE) or b'').decode('latin-1')
        nrows = len(re.findall(r'^app\t', cache2, re.M))
        ck('7 the cache holds N app rows and A0Late', bool(m) and nrows == int(m.group(1)) and 'A0Late' in cache2 and cache2 != old,
           '%d app rows' % nrows)
        x9_leave()
        x9_open_menu()
        x9_level('Apps')
        x9_type('a0late')
        ck('7 the Apps level lists the new row', 'A0Late' in screen_text())
        x9_leave()
        # step 7, GUI: the drawn menu's "watch" sentinel (0 none, 1 starting, 2 running, 3 done, 4 failed)
        import amarchy_gui_rig as g
        c.run('Delete >NIL: ENV:watch ENV:AMARCHY_ROWS QUIET')
        s_, w_ = g.open_menu()
        ck('7 GUI menu opens', bool(w_))
        if w_:
            rows = ''
            for _ in range(40):
                rc, out = c.run('GetEnv AMARCHY_ROWS')
                rows = out.strip() if rc == 0 else ''
                if rows:
                    break
                time.sleep(0.25)
            rl = rows.split('|')
            if 'Apps' in rl:
                for _ in range(rl.index('Apps')):
                    ami.key(RAW['down'])
                    time.sleep(0.3)
                ami.key(RAW['return'])
                time.sleep(1)
            x9_type('rescan')
            ami.key(RAW['return'])
            states, end = [], time.time() + 90
            while time.time() < end:
                rc, out = c.run('GetEnv watch')
                v = out.strip() if rc == 0 else ''
                if v and (not states or states[-1] != v):
                    states.append(v)
                if v in ('3', '4'):
                    break
                time.sleep(0.5)
            ck('7 GUI watch sentinel ends at 3 (done)', states[-1:] == ['3'], 'states %s' % states)
            ck('7 GUI menu window stays open', g.where(g.TITLE)[1] is not None)
            rc, out = c.run('GetEnv AMARCHY_ROWS')
            ck('7 GUI keeps the status row out of AMARCHY_ROWS', 'Scan' not in out, out.strip()[:100])
        g.close_menu()
        # step 7b (RESCAN-ONE-SCANNER): one amarchy-index owns the record and the apps file. A second copy
        # refuses while a scan runs, and Rescan in the GUI shows that scan instead of starting another.
        def scanners():
            return sum('amarchy-index' in l for l in c.run('Status')[1].splitlines())
        c.run('Run >NIL: %s/amarchy-index' % paths.UPTERM_BIN)
        running = False
        for _ in range(20):
            if 'amarchy-index: running' in c.run('%s/amarchy-index --where' % paths.UPTERM_BIN, 30)[1]:
                running = True
                break
            time.sleep(0.25)
        ck('7b the first scan runs', running)
        rc, out = c.run('%s/amarchy-index' % paths.UPTERM_BIN, 30)
        ck('7b a second amarchy-index refuses while the scan runs', rc == 5 and 'a scan runs already' in out,
           'rc %s: %s' % (rc, out.strip()[:100]))
        c.run('Delete >NIL: ENV:watch ENV:AMARCHY_ROWS QUIET')
        s_, w_ = g.open_menu()
        if w_:
            for _ in range(40):
                if c.run('GetEnv AMARCHY_ROWS')[0] == 0:
                    break
                time.sleep(0.25)
            ami.key(RAW['return'])          # into Apps, the first row
            time.sleep(1)
            x9_type('rescan')
            ami.key(RAW['return'])
            time.sleep(1)
            rc, out = c.run('GetEnv watch')
            ck('7b GUI Rescan during a scan shows it running (watch 2)', rc == 0 and out.strip() == '2', out.strip())
            ck('7b one amarchy-index process after the second Rescan', scanners() == 1, '%d' % scanners())
        else:
            ck('7b GUI menu opens', False)
        g.close_menu()
        end = time.time() + 120
        while scanners() and time.time() < end:
            time.sleep(1)
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


def r_app_categories():
    """SYS-CATEGORIES: the Apps and Games levels list the first drawer under the volume as a group; the apps
    sit inside it (the host test tests/test_keymap.c holds merge, volume-root and single-app rules)."""
    import amarchy_rig
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
        x9_conf('amarchy-index-skip = %s' % X9_SKIP)
        c.run('Echo >ENV:up-term/keymap.user "[menu]"')
        c.run('Echo >>ENV:up-term/keymap.user "apps.a0shadow = A0Shadow | run:C:Echo >RAM:x9.menurow hit"')
        amarchy_rig.arexx('RELOAD')
        c.run('Delete >NIL: %s QUIET' % X9_CACHE)
        c.run('%s/amarchy-index' % paths.UPTERM_BIN, 120)
        ck('menu opens', x9_open_menu())
        x9_level('Apps')
        x9_type('x9test')                       # the group name matches; the fake apps' labels do not
        top = screen_text()
        ck('Apps level has the category group X9Test', 'X9Test' in top and 'A0Fake' not in top, ' '.join(menu_rows(top))[:100])
        ami.key(RAW['return'])                  # into the category
        time.sleep(1)
        inside = screen_text()
        ck('the category holds the app (the shadowed one stays a [menu] row at Apps)', 'A0Fake' in inside and 'A0Shadow' not in inside, ' '.join(menu_rows(inside))[:100])
        ck('the category does not hold the game', 'A0Game' not in inside)
        x9_leave()
        ck('menu reopens', x9_open_menu())
        x9_level('Apps')
        x9_type('a0shadow')
        shadow = screen_text()
        ck('the [menu] row stays at the Apps level, once', len(re.findall('A0Shadow', shadow)) == 1, ' '.join(menu_rows(shadow))[:100])
        x9_leave()
        ck('menu reopens for Games', x9_open_menu())
        x9_level('Games')
        x9_type('x9test')
        ami.key(RAW['return'])
        time.sleep(1)
        games = screen_text()
        ck('Games level: the category holds the game', 'A0Game' in games, ' '.join(menu_rows(games))[:100])
    finally:
        x9_end(saved, upterm)
    return all(res), '%d of %d checks passed' % (sum(res), len(res))


def r_group_icons():
    """GROUP-DRAWER-ICON: every group row of the drawn menu (amarchy-gui, Left Amiga + Space) shows a glyph in its
    icon cell. A staged cache (a game, a demo, a song and two apps in the drawers RigAlpha and RigBeta) gives the root
    the groups Apps, Games, Demos and Music and the Apps level two scanned drawers. Each group is brought to row 0 by
    the filter (a root with the filter lists the matching groups first) and its icon cell compared by ink mask
    (pixels that differ from the row's background; the selected row has other colours, the shape is what differs):
    the four kind glyphs must all be inked and pairwise different, and the scanned drawer must be inked and differ
    from the four (the folder). Sentinel: ENV:AMARCHY_DRAWS (gui.c, full draws) is deleted first and above 0 after."""
    import amarchy_rig, amarchy_gui_rig as g
    import tempfile
    res = []

    def ck(name, ok, detail=''):
        res.append(ok)
        print('  %s %s%s' % ('[OK]' if ok else '[FAIL]', name, (': ' + detail) if detail else ''), flush=True)
        return ok

    if not amarchy_rig.port_up():
        r_start()
    ensure_terminal()
    geom = g.build_geom()
    e = g.expected(geom, '-')
    saved, upterm = x9_begin()
    try:
        x9_conf()
        c.run('Echo >ENV:up-term/keymap.user "[menu]"')
        amarchy_rig.arexx('RELOAD')
        cache = ('app\tRigApp1\tWork:RigAlpha/RigApp1\napp\tRigApp2\tWork:RigBeta/RigApp2\n'
                 'game\tRigGame\tWork:Games/RigGame\ngame\tRigDemo\tWork:Demos/RigDemo\n'
                 'app\tRigSong\tWork:Music/RigSong\n')
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / 'apps'
            f.write_text(cache)
            ami.put(str(f), X9_CACHE)
        c.run('Delete >NIL: ENV:AMARCHY_DRAWS QUIET')
        ami.click_px(100, 100)
        time.sleep(0.5)
        s, w = g.open_menu()
        if not ck('drawn menu opened', bool(w)):
            return False, 'the drawn menu did not open'
        time.sleep(2)

        def mask(level, filt):
            """The ink mask of the icon cell of row 0 with the filter `filt` typed (filter cleared after)."""
            x9_type(filt)
            time.sleep(1)
            s2, w2 = g.where(g.TITLE)
            ix, iy, cw, ch, _ = e[level]['cell']
            rows, bg = g.cell_grid(s2, g.box_of(s2, w2), ix, iy, cw, ch)
            ami.key(RAW['esc'])
            time.sleep(0.8)
            return tuple(tuple(r[x * 3:x * 3 + 3] != bytes(bg) for x in range(len(r) // 3)) for r in rows)

        def ink(m):
            return sum(sum(r) for r in m)

        kinds = {}
        for name, filt in (('Apps', 'apps'), ('Games', 'games'), ('Demos', 'demos'), ('Music', 'music')):
            kinds[name] = mask('root', filt)
            ck('root %s group: icon cell holds pixels' % name, ink(kinds[name]) >= 4, '%d pixels' % ink(kinds[name]))
        names = list(kinds)
        distinct = all(kinds[a] != kinds[b] for i, a in enumerate(names) for b in names[i + 1:])
        ck('the four kind glyphs are pairwise different', distinct)
        ami.key(RAW['return'])                  # root row 0 is Apps again after the filter was cleared
        time.sleep(2)
        drawer = mask('apps', 'rigalpha')
        ck('scanned drawer group RigAlpha: icon cell holds pixels', ink(drawer) >= 4, '%d pixels' % ink(drawer))
        ck('the drawer glyph differs from the four kind glyphs', all(drawer != m for m in kinds.values()))
        rc, out = c.run('GetEnv AMARCHY_DRAWS')
        m = re.match(r'\s*(\d+)\s*$', out) if rc == 0 else None
        ck('sentinel: ENV:AMARCHY_DRAWS = %s (the drawn menu drew)' % (m.group(1) if m else 'unset'), bool(m) and int(m.group(1)) > 0)
    finally:
        g.close_menu()
        x9_end(saved, upterm)
    return all(res), '%d of %d checks passed' % (sum(res), len(res))


DOPUS_SCRIPTS = ('newlister', 'listerhere', 'front', 'back', 'parent', 'refresh', 'devicelist', 'ftp', 'configure')
DOPUS_REXX = pathlib.Path(__file__).resolve().parents[2] / 'dist' / 'rexx'


def dopus_port():
    """True when the public port DOPUS.1 exists on the rig (Directory Opus 5 running)."""
    rc, out = c.run(RX_CMD + ' "SAY SHOW(\'P\',\'DOPUS.1\')"')
    return rc == 0 and out.strip().endswith('1')


def dopus_windows():
    """(screen title, window title) of every window open now, to see which screen a new one lands on. The Directory Opus
    screen title carries a live memory count after two spaces; only the name before it names the screen."""
    m = ami._m()
    with ami._conn() as conn:
        return [(re.split(r'\s{2,}', ((s or {}).get('title') if isinstance(s, dict) else s) or '')[0], w.get('title') or '')
                for s, w in m.ui_windows(m.read_ui(conn, every_screen=True))]


def new_windows(before):
    """The windows open now that were not in `before`, counted (a second lister has the title of the first)."""
    import collections
    return list((collections.Counter(dopus_windows()) - collections.Counter(before)).elements())


def new_titled(before):
    """Like new_windows, but by window title alone: the screen name flips between "Directory Opus" and "Workbench Screen"
    with the active window, and the first lister also brings DOpus's own 'menuhost' and untitled helper windows, which
    are not what a hotkey or a row opens. Returns the titles of the new real windows."""
    import collections
    keep = lambda ws: collections.Counter(t for _s, t in ws if t not in ('', 'menuhost'))
    return list((keep(dopus_windows()) - keep(before)).elements())


def stage_dopus_rexx():
    """Put the dopus-*.rexx scripts into UP-Term:Rexx, where dist/keymap names them (rexx:UP-Term:Rexx/dopus-*.rexx). A rig's
    UP-Term: is the drawer of an older kit install that has no Rexx drawer; a current kit install creates it."""
    c.run('MakeDir UP-Term:Rexx')
    for n in DOPUS_SCRIPTS:
        ami.put(str(DOPUS_REXX / ('dopus-%s.rexx' % n)), 'UP-Term:Rexx/dopus-%s.rexx' % n)


# The rig's shell path is C: only (rig.py startup), so RX is named by its drawer; a real Workbench has SYS:Rexxc in the path.
RX_CMD = 'SYS:Rexxc/rx'


def r_dopus_absent():
    """DOPUS-ABSENT (plan 2026-10-25-amarchy-dopus-integration.md D5, D3): with no DOPUS.1 port every dist/rexx/dopus-*.rexx
    script ends with RC 5 and opens no window, and the drawn root menu lists no Dopus group. Needs a rig that has no
    Directory Opus running (a rig without DOpus, or one where it is quit)."""
    if dopus_port():
        return False, 'DOPUS.1 exists on this rig: this case needs no Directory Opus running'
    res = []
    c.run('Delete >NIL: RAM:x9.dopus-#? QUIET')
    before = dopus_windows()
    for n in DOPUS_SCRIPTS:
        ami.put(str(DOPUS_REXX / ('dopus-%s.rexx' % n)), 'RAM:x9.dopus-%s.rexx' % n)
        rc, out = c.run(RX_CMD + ' RAM:x9.dopus-%s.rexx' % n)
        print('  %s dopus-%s.rexx without a port: RC %s' % ('[OK]' if rc == 5 else '[FAIL]', n, rc), flush=True)
        res.append(rc == 5)
    time.sleep(1)
    after = dopus_windows()
    res.append(sorted(after) == sorted(before))
    print('  %s windows before %d, after %d' % ('[OK]' if res[-1] else '[FAIL]', len(before), len(after)), flush=True)
    c.run('Delete >NIL: RAM:x9.dopus-#? QUIET')
    open_terminal()
    up, rows = root_menu_rows()
    res.append(bool(up) and 'Apps' in rows and 'Dopus' not in rows)
    print('  %s root rows without a port: %s' % ('[OK]' if res[-1] else '[FAIL]', rows), flush=True)
    return all(res), '%d of %d checks passed' % (sum(res), len(res))


def r_dopus_present():
    """DOPUS-PRESENT (D3, D6, D7): with Directory Opus 5 running (DOPUS.1, `dopus version` first word 5) the root menu lists
    Dopus, Super+Shift+D adds one lister (count from `lister query all`, read through the port), and Open UP-Term here opens
    its window on the Directory Opus screen. Blocked, not failed, on a rig where DOpus is not installed: the verdict
    says so and stays FAIL until a rig with DOpus 5.104 runs it."""
    if not dopus_port():
        return False, 'BLOCKED: DOPUS.1 not on this rig (Directory Opus 5.104 not installed here)'
    res = []
    stage_dopus_rexx()
    ensure_terminal()
    up, rows = root_menu_rows()
    res.append(bool(up) and 'Dopus' in rows)
    print('  %s root rows with a port: %s' % ('[OK]' if res[-1] else '[FAIL]', rows), flush=True)
    c.run('Delete >NIL: ENV:AMARCHY_DRAWS QUIET')
    # `lister query all` answers nothing on 5.82 and 5.91, so the sentinel is the window list: one new window, a lister
    before = dopus_windows()
    ami.key(RAW['d'], LAMIGA | LSHIFT)
    time.sleep(3)
    listers = new_titled(before)
    res.append(len(listers) == 1)
    print('  %s Super+Shift+D: new windows (title) %s' % ('[OK]' if res[-1] else '[FAIL]', listers), flush=True)
    before = dopus_windows()
    import amarchy_gui_rig as g
    s, w = g.open_menu()
    # the filter works on the level shown: the group at the root, then its row inside the group (a contiguous
    # case-insensitive substring, amarchy/menu_core.c mn_match)
    for label in ('Dopus', 'Open UP-Term here'):
        x9_type(label)
        ami.key(RAW['return'])
        time.sleep(1)
    time.sleep(3)
    new = [t for t in new_titled(before) if t != 'Amarchy']
    # screen titles follow the active window, so they name no screen; the window title UP-Term is the proof
    res.append(new == ['UP-Term'])
    print('  %s Open UP-Term here: new windows (title) %s' % ('[OK]' if res[-1] else '[FAIL]', new), flush=True)
    return all(res), '%d of %d checks passed' % (sum(res), len(res))


THEME_DIR = pathlib.Path(__file__).resolve().parents[2] / 'dist' / 'amarchy-themes'
GT_FIXTURES = {'kbg': ('background', '#103020'), 'kfg': ('foreground', '#e0a0a0'),
               'kdf': ('dark_foreground', '#20e0e0'), 'kac': ('accent', '#e02080')}


def r_gtheme():
    """GUI-THEME reachability (plan 2026-10-26-amarchy-gui-themes.md, T15/T6/T7/T20): the drawn menu (Left Amiga +
    Space) takes its pens from the Omarchy GUI theme. Fixtures in ENVARC:Amarchy/themes: nord and gruvbox as shipped, and
    four copies of nord that change one key each (kbg background, kfg foreground, kdf dark_foreground, kac accent).
    Sentinel ENV:AMARCHY_THEME (gui.c pens_get) names the theme that drew; it is deleted before every open and read
    after the menu closes (no Amiga command runs while the menu window is open, so the UI list stays small).
    T15: the theme is picked through the menu (Style > Theme > <name>, filter typing) and the next open draws it; the
    terminal window's pixels are the same before and after. T20: each of the four keys shows up as pixels of its colour
    that the nord baseline does not have. T6: fg/bg keys of the upconf do not reach the drawn menu. T7: minimal style
    draws the theme's background and foreground."""
    import collections, amarchy_rig, amarchy_gui_rig as g
    res = []

    def ck(name, ok, detail=''):
        res.append(ok)
        print('  %s %s%s' % ('[OK]' if ok else '[FAIL]', name, (': ' + detail) if detail else ''), flush=True)
        return ok

    files = {n: (THEME_DIR / (n + '.theme')).read_text() for n in ('nord', 'gruvbox')}
    for n, (key, val) in GT_FIXTURES.items():
        files[n] = re.sub(r'^%s = ".*"' % key, '%s = "%s"' % (key, val), files['nord'], flags=re.M)

    def role(n, key):
        return bytes.fromhex(re.search(r'^%s = "#(\w+)"' % key, files[n], re.M).group(1))

    def window_hist(title):
        s, w = g.where(title)
        if not w:
            return None
        left, top, ww, hh = g.box_of(s, w)
        gw, gh, rows = ami.grab((left + 6, top + 6, ww - 12, hh - 12))
        return collections.Counter(bytes(r[x * 3:x * 3 + 3]) for r in rows for x in range(len(r) // 3))

    def host_hist():
        titles = [w['title'] for s in g.screens() for w in s['windows'] if 'menuhost' in (w['title'] or '')]
        return window_hist(titles[0]) if titles else None

    def prefs(theme, style, extra=()):
        # menu-in-terminal: the click below lands in the host terminal
        x9_conf('amarchy-gui-theme = %s' % theme, 'menu-style = %s' % style, 'menu-in-terminal = graphical', *extra)
        amarchy_rig.arexx('RELOAD')

    def open_drawn():
        c.run('Delete >NIL: ENV:AMARCHY_THEME ENV:AMARCHY_DRAWS QUIET')
        ami.click_px(100, 100)
        time.sleep(0.5)
        s, w = g.open_menu()
        time.sleep(2)
        return bool(w)

    def sentinel():
        return c.run('GetEnv AMARCHY_THEME')[1].strip()

    def draw(theme, style='graphical', extra=()):
        # one open: pixels while it is open, sentinels once closed
        prefs(theme, style, extra)
        up = open_drawn()
        h = window_hist(g.TITLE) or collections.Counter()
        g.close_menu()
        return up, h, sentinel()

    if not amarchy_rig.port_up():
        r_start()
    ensure_terminal()
    saved, upterm = x9_begin()
    try:
        c.run('Echo >ENV:up-term/keymap.user "[menu]"')
        c.run('MakeDir >NIL: ENVARC:Amarchy ENVARC:Amarchy/themes ENV:Amarchy')
        for n, t in files.items():
            f = OUT / ('gtheme_' + n + '.theme')
            f.write_text(t)
            ami.put(str(f), 'ENVARC:Amarchy/themes/%s.theme' % n)
        try:                                   # a fresh commodity reads the fixtures and the assign
            amarchy_rig.arexx('QUIT')
        except Exception:
            pass
        time.sleep(2)
        r_start()
        time.sleep(3)
        ck('commodity port up', amarchy_rig.port_up())
        ami.key(g.ESC)                         # a text menu left open by an earlier run
        time.sleep(1)
        base = None
        for n in ('nord', 'gruvbox', 'kbg', 'kfg', 'kdf', 'kac'):
            up, h, th = draw(n)
            ck('%s: drawn menu opened, AMARCHY_THEME = %r, draws %d' % (n, th, g.draws()), up and th == n and g.draws() >= 1)
            if n == 'nord':
                base = h
            bg, fg = role(n, 'background'), role(n, 'foreground')
            ck('%s: background %s px %d, foreground %s px %d' % (n, bg.hex(), h[bg], fg.hex(), h[fg]), h[bg] > 0 and h[fg] > 0)
            if n in GT_FIXTURES:
                key = GT_FIXTURES[n][0]
                col = role(n, key)
                ck('T20 %s: changed %s %s px %d (nord baseline %d)' % (n, key, col.hex(), h[col], base[col]),
                   h[col] > 0 and base[col] == 0)
            if n == 'gruvbox':
                nb = role('nord', 'background')
                ck('gruvbox: nord background %s px %d (0 expected)' % (nb.hex(), h[nb]), h[nb] == 0)
        # T6: fg/bg keys of the upconf do not reach the drawn menu
        up, h, th = draw('nord', 'graphical', ('fg = E0E0A0', 'bg = 102040'))
        pf, pb = bytes.fromhex('e0e0a0'), bytes.fromhex('102040')
        ck('T6 upconf fg E0E0A0 px %d, bg 102040 px %d (0 expected), theme %r' % (h[pf], h[pb], th),
           up and h[pf] == 0 and h[pb] == 0 and th == 'nord')
        # T7: minimal style follows the theme
        up, h, th = draw('gruvbox', 'minimal')
        gb, gf = role('gruvbox', 'background'), role('gruvbox', 'foreground')
        ck('T7 minimal: theme %r, mode %d, pens %d, background px %d, foreground px %d' % (
            th, g.env_count('AMARCHY_MODE'), g.env_count('AMARCHY_PENS'), h[gb], h[gf]),
           up and th == 'gruvbox' and g.env_count('AMARCHY_MODE') == 2 and h[gb] > 0 and h[gf] > 0)
        # T15: pick Style > Theme > kbg through the drawn menu (the filter typed at each level), reopen, read the sentinel
        prefs('nord', 'graphical')
        before = host_hist()
        open_drawn()
        for word in ('Style', 'Theme', 'kbg'):
            x9_type(word)
            ami.key(RAW['return'])
            time.sleep(1.5)
        g.close_menu()
        up = open_drawn()
        h = window_hist(g.TITLE) or collections.Counter()
        g.close_menu()
        th, kb = sentinel(), role('kbg', 'background')
        ck('T15 picked through the menu: next open AMARCHY_THEME = %r, kbg background %s px %d' % (th, kb.hex(), h[kb]),
           up and th == 'kbg' and h[kb] > 0)
        after = host_hist()
        ck('T15 terminal window pixels unchanged by the pick', before is not None and before == after,
           'colours before %d, after %d' % (len(before or ()), len(after or ())))
    finally:
        g.close_menu()
        c.run('Delete >NIL: ENVARC:Amarchy/themes/#?.theme QUIET')
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
    ('app_categories', r_app_categories),
    ('group_icons', r_group_icons),
    ('dopus_absent', r_dopus_absent),
    ('dopus_present', r_dopus_present),
    ('minimal',     r_minimal),
    ('gtheme',      r_gtheme),
]


ICON_ROWS = ('app_icons', 'group_icons')     # rows that need icon.library V44 (OS 3.2), see r_app_icons
OS32_NAMES = ('32', '321', '322')


def start_args():
    """The arguments the running rig was started with (rig.py writes them to <rig>/start.args); [] when unknown."""
    try:
        return json.loads((paths.RIG / 'start.args').read_text())
    except (OSError, ValueError):
        return []


def is_os32(args):
    """True when a rig.py start argument list boots AmigaOS 3.2 (--os32, or --os 32 / 321 / 322)."""
    return '--os32' in args or ('--os' in args[:-1] and args[args.index('--os') + 1] in OS32_NAMES)


def os32_start_args(args):
    """The last start's arguments with the OS switched to 3.2 (the same reboot_args idea as rig.py screenmode:
    keep the machine flags and the purpose, replace --os / --os32)."""
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
        elif a == '--os':
            skip = True
        elif a != '--os32':
            out.append(a)
    return out + ['--os', '32']


def ensure_os32_rig():
    """The rows in ICON_ROWS run on an OS 3.2 rig. A rig started otherwise is restarted with the same flags and
    --os 32 (rig.py stop, then start, as rig.py screenmode does). Returns an error text, '' when the rig is OS 3.2.
    Rig 1 is the owner's and is never stopped here."""
    args = start_args()
    if is_os32(args):
        return ''
    if paths.RIG_N == 1:
        return 'rig 1 is not an OS 3.2 rig and is never restarted here; use another rig (UPTERM_RIG=<n>) or rig.py start --os 32'
    rig = [sys.executable, '-I', str(ROOT / 'tools/rig/rig.py')]
    new = os32_start_args(args or ['--max'])
    print('amarchy_menu_rig: restarting rig %d on OS 3.2: rig.py start %s' % (paths.RIG_N, ' '.join(new)), flush=True)
    subprocess.run(rig + ['stop'], check=True)
    subprocess.run(rig + ['start'] + new, check=True)
    return '' if is_os32(start_args()) else 'rig %d did not come up on OS 3.2' % paths.RIG_N


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true', help='stage what build/amiga holds, do not run make')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if any(n in ICON_ROWS and (not a.only or a.only == n) for n, _ in ROWS):
        err = ensure_os32_rig()
        if err:
            print('FAIL app_icons: ' + err)
            return 1
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
