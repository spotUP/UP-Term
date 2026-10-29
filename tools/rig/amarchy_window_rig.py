#!/usr/bin/env python3
"""amarchy_window_rig.py -- the win: verbs of the Amarchy commodity (plan 2026-10-25-amarchy-window-groups.md,
Phase R, rows K5-K12; reachability section 7). Resumable like amarchy_rig.py: a verdict file per step in
<RIG>/amarchy-window/<step>.verdict; --only <step>, --failed, --fresh.

Entry point and sentinel: the commodity's own dispatch. A known window (NewShell CON:40/40/320/120/amarchy-target)
is moved with the win: verb; STATUS `wins` (K9) proves the new code ran and the window's LeftEdge read back through
the same window list the wasabi MCP amiga_windows reports proves the effect (32 pixels, other box fields kept).
The move-right step presses the real hotkey (Left Amiga + Shift + Right Arrow, the shipped win:move right row).
The menu_move_left step opens the drawn menu (Super+Space) with the target active, picks Window then Move left, and
so drives the ticket (K5a) and the LF_WIN leaf (K8, WIN verb over the port). The rc 5 ticket-box refusal (K5a step c)
has no step: the menu window is active while it is open, so no input changes the target box without ending it.
The K6 probe sends each held-lock verb (front, back, zip, move, size, fullwidth) and then a STATUS that must answer
within 5 s: a deadlock of LockIBase with the queued Intuition call would leave the commodity silent.
Needs the rig up (UPTERM_RIG=<n> rig.py start --max --purpose ...). Run from the checkout whose build is under
test: python3 tools/rig/amarchy_window_rig.py [--no-build]"""
import argparse, pathlib, re, sys, threading, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig

DIST = pathlib.Path(__file__).resolve().parents[2] / 'dist'
OUT = pathlib.Path(paths.RIG) / 'amarchy-window'
TARGET = 'amarchy-target'
SPEC = 'CON:40/40/320/120/' + TARGET
STEP = 32                      # win_core.h WIN_STEP
RAW_RIGHT, LAMIGA, LSHIFT = 0x4E, 0x40, 0x01
SPACE, MENU_TITLE = 0x40, 'Amarchy'
ENTER = b'\r'


def port(cmd, timeout=30):
    """(rc, result) of one command to port AMARCHY; (None, '') when there is no port"""
    m = ami._m()
    try:
        with ami._conn() as conn:
            rc, _, res = m.arexx(conn, 'AMARCHY', cmd, timeout=timeout)
            return rc, res.strip()
    except m.WasabiError:
        return None, ''


def status():
    d = {}
    for w in port('STATUS')[1].replace('\n', ' ').split():
        if '=' in w:
            k, v = w.split('=', 1)
            if v.lstrip('-').isdigit():
                d[k] = int(v)
    return d


def box():
    w = ami.window(TARGET)
    return w['box'] if w else None


def answers_within(seconds):
    """STATUS answers within `seconds` (a deadlocked commodity does not)"""
    got = []
    t = threading.Thread(target=lambda: got.append(port('STATUS', timeout=seconds)), daemon=True)
    t.start()
    t.join(seconds)
    return bool(got) and got[0][0] == 0 and 'wins=' in got[0][1]


def activate_target():
    """the target is the active window: a click on its title bar"""
    b = box()
    if b:
        ami.click_px(b[0] + 60, b[1] + 5)
        time.sleep(0.5)


STATE = {}


def s_start():
    port('QUIT')
    time.sleep(1.5)
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    time.sleep(2.5)
    s = status()
    return 'wins' in s and s['wins'] == 0, 'STATUS %r' % s


def s_open_target():
    c.run('Run >NIL: NewShell "%s"' % SPEC)
    time.sleep(2)
    activate_target()
    w = ami.window(TARGET)
    STATE['box0'] = w['box'] if w else None
    return bool(w), 'window %r' % (w,)


def s_hotkey_move_right():
    b0 = STATE.get('box0')
    if not b0:
        return False, 'no target window'
    n0 = status().get('wins', -1)
    ami.key(RAW_RIGHT, LAMIGA | LSHIFT)
    how = 'real hotkey Super+Shift+Right'
    time.sleep(1)
    b1, n1 = box(), status().get('wins', -1)
    ok = b1 is not None and b1[0] == b0[0] + STEP and b1[1:] == b0[1:] and n1 == n0 + 1
    return ok, '%s: box %r -> %r, wins %d -> %d' % (how, b0, b1, n0, n1)


PREFS = ('ENV:Amarchy/prefs', 'ENVARC:Amarchy/prefs')


def put_text(path, lines):
    """Rewrite a prefs file line by line with Echo (as amarchy_menu_rig.x9_conf does): ami.restore_file's rename
    into place fails on ENV: of the OS 3.2 rig."""
    c.run('Delete >NIL: %s QUIET' % path)
    for i, l in enumerate(lines):
        c.run('Echo >%s%s "%s"' % ('>' if i else '', path, l.replace('"', '*"')))


def drawn_menu_prefs():
    """Super+Space over a terminal (the target is a console) opens amarchy-menu in an untitled UP-Term window while
    menu-in-terminal = text (the default); the drawn menu, titled Amarchy, needs graphical. Same store and key as
    amarchy_prefs_rig.set_graphical. Returns the saved files for restore_prefs."""
    saved = {p: ami.read_file(p) for p in PREFS}
    for p, old in saved.items():
        keep = [l for l in (old or b'').decode('latin-1').splitlines()
                if not re.match(r'\s*menu-in-terminal\s*=', l)]
        put_text(p, keep + ['menu-in-terminal = graphical'])
    port('RELOAD')
    time.sleep(1.5)
    return saved


def restore_prefs(saved):
    for p, old in saved.items():
        if old is None:
            c.run('Delete >NIL: %s QUIET' % p)
        else:
            put_text(p, old.decode('latin-1').splitlines())
    port('RELOAD')


def s_menu_move_left():
    """drawn menu: Super+Space, Window, Move left; the ticket names the target although the menu is active"""
    if 'box0' not in STATE:
        return False, 'needs step open_target in the same run (STATE box0 unset); rerun with --fresh'
    saved = drawn_menu_prefs()
    try:
        return menu_move_left_run()
    finally:
        restore_prefs(saved)


def menu_move_left_run():
    n0, b0 = status().get('wins', -1), box()
    activate_target()
    ami.key(SPACE, LAMIGA)
    end = time.time() + 15
    while time.time() < end and not ami.window(MENU_TITLE):
        time.sleep(0.5)
    if not ami.window(MENU_TITLE):
        return False, 'no menu window titled %r' % MENU_TITLE
    time.sleep(1)
    with ami._conn() as conn:
        for text in (b'Window', b'Move left'):
            ami._type(conn, text)
            time.sleep(0.7)
            ami._type(conn, ENTER)
            time.sleep(1)
    time.sleep(2)
    b1, n1 = box(), status().get('wins', -1)
    still = bool(ami.window(MENU_TITLE))
    for _ in range(3 if still else 0):        # never leave the menu open for the next step
        ami.key(0x45, 0)
        time.sleep(0.7)
    ok = b0 is not None and b1 == STATE['box0'] and b1 != b0 and n1 == n0 + 1
    return ok, 'menu Window > Move left: box %r -> %r (first %r), wins %d -> %d, menu open: %s' % (
        b0, b1, STATE['box0'], n0, n1, still)


def s_move_left():
    if 'box0' not in STATE:
        return False, 'needs step open_target in the same run (STATE box0 unset); rerun with --fresh'
    port('WIN move right')                    # the menu step already moved the window back
    time.sleep(1)
    b0, n0 = box(), status().get('wins', -1)
    rc, _ = port('WIN move left')
    time.sleep(1)
    b1, n1 = box(), status().get('wins', -1)
    ok = rc == 0 and b1 == STATE['box0'] and n1 == n0 + 1
    return ok, 'rc %r box %r -> %r (first %r), wins %d -> %d' % (rc, b0, b1, STATE['box0'], n0, n1)


def s_refuse_and_unknown():
    n0 = status().get('wins', -1)
    rc_bad, _ = port('WIN teleport')
    n1 = status().get('wins', -1)
    return rc_bad == 10 and n1 == n0, 'unknown verb rc %r, wins %d -> %d (not counted)' % (rc_bad, n0, n1)


def probe(verb, undo=None):
    def step():
        activate_target()
        rc, _ = port('WIN ' + verb, timeout=5)
        ok = answers_within(5)
        if undo:
            port('WIN ' + undo, timeout=5)
        return rc == 0 and ok, 'WIN %s rc %r, STATUS answered within 5 s: %s' % (verb, rc, ok)
    return step


def front_screen():
    """the screens list, front first, as a tuple of titles (a second screen of the same title still changes the tuple)"""
    b = ami.req(0x0A)
    out, off = [], 1
    for _ in range(b[0] if b else 0):
        ln = b[off + 10]
        out.append(b[off + 11:off + 11 + ln].decode('latin-1'))
        off += 11 + ln
    return tuple(out)


def wb_port(cmd, timeout=15):
    """(rc, result) of one command to port WORKBENCH; (None, error) when it fails"""
    m = ami._m()
    try:
        with ami._conn() as conn:
            rc, _, res = m.arexx(conn, 'WORKBENCH', cmd, timeout=timeout)
            return rc, res.strip()
    except m.WasabiError as e:
        return None, 'error %s' % (e,)


def needs_wb(fn):
    """The b2 steps drive Workbench's own ARexx port. A rig where Directory Opus replaces Workbench has none: FAIL
    with the reason (never a silent pass); run these steps on a rig started with --no-dopus."""
    def run():
        rc, res = wb_port('ACTIVATEWINDOW WINDOW ROOT')
        if rc is None and 'no port named' in res:
            return False, 'needs a WORKBENCH port (Workbench rig: rig.py start --no-dopus; DOpus replaces Workbench here): %s' % res
        return fn()
    return run


def wb_rows():
    """(id, scope, item) of every workbench.* row in dist/keymap"""
    rows = []
    path = pathlib.Path(__file__).resolve().parents[2] / 'dist' / 'keymap'
    for line in path.read_text(encoding='latin-1').splitlines():
        m = re.match(r"(workbench\.\w+)\s*=.*MENU WINDOW (ROOT|ACTIVE) INVOKE (\S+)'", line)
        if m:
            rows.append(m.groups())
    return rows


def s_screen_cycle():
    """S2: with the UP-Term own screen up, win:screen moves the front screen, read back from the screens list"""
    c.run('Run >NIL: NewShell "XCON:0/20/400/100/own/OWNSCREEN"')
    time.sleep(4)
    log, ok = ['front %r' % (front_screen(),)], True
    for verb in ('workbench', 'next', 'next', 'prev', 'back', 'workbench'):
        before = front_screen()
        rc, _ = port('WIN screen ' + verb, timeout=10)
        time.sleep(1.5)
        after = front_screen()
        log.append('%s rc %r: %r -> %r' % (verb, rc, before, after))
        ok = ok and rc == 0 and (after != before or verb == 'workbench' and 'Workbench' in after[0])
    return ok, '; '.join(log)


def s_wb_redraw():
    rc, res = wb_port('MENU WINDOW ROOT INVOKE WORKBENCH.REDRAWALL')
    return rc == 0, 'workbench.redrawall rc %r %r' % (rc, res)


def s_wb_viewname():
    """B2: workbench.viewname through WINDOW ACTIVE reads back as View by / Name [checked] in the menu strip"""
    wb_port('WINDOW SYS: OPEN')
    time.sleep(1)
    wb_port('ACTIVATEWINDOW WINDOW SYS:')
    time.sleep(1)
    rc, res = wb_port('MENU WINDOW ACTIVE INVOKE WINDOW.VIEWBY.NAME')
    time.sleep(1)
    got = [l for l in ami.req(0x0F, b'').decode('latin-1').splitlines() if l.startswith('I 1 10 ') and '"Name"' in l]
    return rc == 0 and any(' checked ' in l for l in got), 'rc %r %r; menu rows %r' % (rc, res, got)


# Rows that need a selected icon (Icons menu) or an active drawer (Auto-arrange Icons); b2_icons_selected owns them
ICON_ROWS = ('workbench.iconopen', 'workbench.iconcopy', 'workbench.iconrename', 'workbench.iconinfo',
             'workbench.iconsnapshot', 'workbench.iconunsnapshot', 'workbench.iconleaveout', 'workbench.iconputaway',
             'workbench.autoarrange')


def s_wb_patterns():
    out = []
    wb_port('WINDOW SYS: OPEN')
    time.sleep(1)
    for rid, scope, item in wb_rows():
        if rid in ('workbench.delete', 'workbench.emptytrash') or rid in ICON_ROWS:
            continue
        wb_port('ACTIVATEWINDOW WINDOW SYS:')
        if not item.startswith('WINDOW.VIEWBY'):
            wb_port('MENU WINDOW ACTIVE INVOKE WINDOW.VIEWBY.ICON')
            time.sleep(0.3)
        if scope == 'ACTIVE':
            wb_port('ICON WINDOW SYS: NAMES Prefs SELECT')    # the Icons menu is disabled with nothing selected
            time.sleep(0.3)
        rc, res = wb_port('MENU WINDOW %s INVOKE %s' % (scope, item))
        out.append('%s %s rc %r %s' % (rid, item, rc, res if rc else ''))
        time.sleep(0.4)
    (OUT / 'wb_patterns.txt').write_text('\n'.join(out) + '\n')
    bad = [o for o in out if ' rc 0' not in o]
    return not bad, '%d sent, %d refused: %s' % (len(out), len(bad), '; '.join(bad))


def ui_windows():
    """{title: [gadget names]} of the front screen's windows, from the uitree text; a volume window's title carries live
    free-space numbers, so those are cut off at the first double space ("RAM Disk  0% full ..." is "RAM Disk")"""
    out, cur = {}, None
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        m = re.match(r'W \d+ \d+ \d+ \S+ \S+ "(.*)"$', line)
        if m:
            cur = out.setdefault(m.group(1).split('  ')[0], [])
        elif cur is not None and line.startswith('G '):
            m = re.match(r'G \d+ \d+ \d+ \S+ (\S+) - "(.*)"$', line)
            if m:
                cur.append(m.group(2) or m.group(1))
    return out


def close_extras(keep):
    """Cancel or close every window whose title is not in `keep` (a requester has a Cancel gadget); returns their titles"""
    closed = []
    for _ in range(12):
        wins = ui_windows()
        extra = [t for t in wins if t not in keep]
        if not extra:
            return closed
        t = extra[0]
        with ami._conn() as conn:
            ami._click_gadget(conn, t, 'Cancel' if 'Cancel' in wins[t] else 'close')
        closed.append(t)
        time.sleep(0.8)
    return closed + ['STILL OPEN %r' % [t for t in ui_windows() if t not in keep]]


def ram_window():
    """geometry '118x65' of the RAM Disk window from the uitree text, None when it is not open"""
    for l in ami.req(0x0D).decode('latin-1').splitlines():
        m = re.match(r'W \d+ -?\d+ -?\d+ (\d+x\d+) \S+ "RAM Disk', l)
        if m:
            return m.group(1)
    return None


def s_wb_icons_selected():
    """B2: the 8 Icons rows with one icon selected in a scratch drawer window in RAM:, then Auto-arrange Icons with a drawer
    window active. Nothing system-owned is touched: the icon is RAM:Scr (a drawer icon copied from SYS:Prefs.info).
    Auto-arrange Icons is a WBStartup AppMenuItem, not a workbench.library menu name: MENU INVOKE TOOLS.AUTOARRANGEICONS
    is rc 10 in every state; the shipped row runs dist/rexx/wb-autoarrange.rexx, which this step runs with a drawer window active."""
    c.run('Delete RAM:Scr RAM:Scr.info RAM:.backdrop RAM:copy_of_Scr#? ALL QUIET')
    c.run('MakeDir RAM:Scr')
    c.run('Copy SYS:Prefs.info RAM:Scr.info')
    wb_port('WINDOW RAM: OPEN')
    wb_port('MENU WINDOW RAM: INVOKE WINDOW.UPDATE')     # the window may be open from before the scratch icon existed
    time.sleep(1.5)
    keep = set(ui_windows())
    out, bad = [], []
    shows = {'workbench.iconopen': 'Scr', 'workbench.iconrename': 'Rename', 'workbench.iconinfo': 'RAM Disk:Scr'}
    for rid, scope, item in wb_rows():
        if rid not in ICON_ROWS or rid == 'workbench.autoarrange':
            continue
        # a left-out icon lives on the Workbench root (RAM:.backdrop names it), so Put Away selects it there
        win = 'ROOT' if rid == 'workbench.iconputaway' else 'RAM:'
        wb_port('ACTIVATEWINDOW WINDOW ' + win)
        if win == 'RAM:':
            wb_port('MENU WINDOW ACTIVE INVOKE WINDOW.VIEWBY.ICON')
            time.sleep(0.3)
        src, _ = wb_port('ICON WINDOW %s NAMES Scr SELECT' % win)
        time.sleep(0.3)
        rc, res = wb_port('MENU WINDOW %s INVOKE %s' % (scope, item))
        time.sleep(1.5)
        opened = [t for t in ui_windows() if t not in keep]
        close_extras(keep)
        c.run('Delete RAM:copy_of_Scr#? ALL QUIET')
        backdrop = c.run('List RAM:.backdrop')[0] == 0
        if rid != 'workbench.iconleaveout':
            wb_port('ICON WINDOW %s NAMES Scr UNSELECT' % win)
        out.append('%s %s select rc %r, invoke rc %r %s; windows opened: %r; RAM:.backdrop %s' % (
            rid, item, src, rc, res if rc else '', opened, 'present' if backdrop else 'absent'))
        if rid == 'workbench.iconleaveout' and not backdrop or rid == 'workbench.iconputaway' and backdrop:
            bad.append(rid + ' effect')
        if rc != 0 or (rid in shows and shows[rid] not in opened):   # the selection really reached the row
            bad.append(rid)
    # the shipped workbench.autoarrange row is the script UP-Term:Rexx/wb-autoarrange.rexx; Amarchy hands a rexx: row to RX
    row = [l for l in (DIST / 'keymap').read_text(encoding='latin-1').splitlines() if l.startswith('workbench.autoarrange')]
    script = row[0].split('rexx:')[1].strip()
    assert script.startswith('UP-Term:Rexx/'), row
    ami.put(str(DIST / 'rexx' / script.split('/')[1]), 'RAM:x.wb-autoarrange.rexx')
    c.run('MakeDir RAM:Scr2')
    c.run('Copy SYS:Prefs.info RAM:Scr2.info')           # two icons, so the fitted window differs from the unarranged one
    wb_port('WINDOW RAM: CLOSE')
    c.run('Delete RAM:Disk.info QUIET')                  # the row snapshots the window: a leftover Disk.info keeps an old size
    time.sleep(0.5)
    wb_port('WINDOW RAM: OPEN')
    wb_port('MENU WINDOW RAM: INVOKE WINDOW.UPDATE')
    wb_port('ACTIVATEWINDOW WINDOW RAM:')
    time.sleep(1.5)
    before = ram_window()
    pre = c.run('List RAM:Disk.info')[0] == 0
    arc, ares = c.run('SYS:Rexxc/rx RAM:x.wb-autoarrange.rexx')
    time.sleep(1.5)
    after = ram_window()
    snap = c.run('List RAM:Disk.info')[0] == 0           # WINDOW.SNAPSHOT.ALL writes the volume window's Disk.info
    wb_port('ACTIVATEWINDOW WINDOW ROOT')
    time.sleep(0.5)
    nrc, _ = c.run('SYS:Rexxc/rx RAM:x.wb-autoarrange.rexx')
    out.append('workbench.autoarrange: %s rc %r %s; RAM: window %r -> %r; RAM:Disk.info %s -> %s; no drawer active rc %r'
               % (script, arc, ares.strip(), before, after, pre, snap, nrc))
    if arc != 0 or pre or not snap or nrc != 5:
        bad.append('workbench.autoarrange')
    close_extras(keep)
    c.run('Delete RAM:Disk.info QUIET')
    c.run('Delete RAM:Scr RAM:Scr.info RAM:Scr2 RAM:Scr2.info RAM:x.wb-autoarrange.rexx RAM:.backdrop RAM:copy_of_Scr#? ALL QUIET')
    (OUT / 'wb_icons_selected.txt').write_text('\n'.join(out) + '\n')
    return not bad, '%d rows, refused: %s' % (len(out), bad)


def target_box():
    """the target must exist and be the active window, else (None, reason)"""
    if 'box0' not in STATE:
        return None
    activate_target()
    return box()


def s_bar_strut():
    """P3-B1: with the bar open (title 'Amarchy bar') fullscreen ends where the bar begins"""
    bar = ami.window('Amarchy bar')
    if not bar or target_box() is None:
        return False, 'no Amarchy bar window or no target (run open_target in the same run): bar %r' % (bar,)
    rc, _ = port('WIN fullscreen')
    time.sleep(1)
    b = box()
    port('WIN restore')
    time.sleep(0.5)
    ok = rc == 0 and b is not None and b[1] + b[3] == bar['box'][1]
    return ok, 'fullscreen box %r, bar box %r (bottom must equal bar top)' % (b, bar['box'])


def s_snap_status():
    """P3-B2: WIN snap left moves the target and STATUS snaps= and wins= rise by one"""
    if target_box() is None:
        return False, 'needs step open_target in the same run'
    s0 = status()
    rc, _ = port('WIN snap left')
    time.sleep(1)
    s1, b = status(), box()
    ok = rc == 0 and s1.get('snaps', -1) == s0.get('snaps', -2) + 1 and s1.get('wins', -1) == s0.get('wins', -2) + 1 \
        and b is not None and b[0] < 40
    return ok, 'rc %r snaps %r -> %r wins %r -> %r box %r' % (rc, s0.get('snaps'), s1.get('snaps'), s0.get('wins'), s1.get('wins'), b)


def with_gap(gap):
    """tile-gap = gap in both prefs files; returns the saved files for restore_prefs"""
    saved = {p: ami.read_file(p) for p in PREFS}
    for p, old in saved.items():
        keep = [l for l in (old or b'').decode('latin-1').splitlines() if not re.match(r'\s*tile-gap\s*=', l)]
        put_text(p, keep + ['tile-gap = %d' % gap])
    port('RELOAD')
    time.sleep(1.5)
    return saved


def s_snap_gap():
    """P3-B3: tile-gap 0 and 8 give different snap boxes; the file is read at each snap"""
    if target_box() is None:
        return False, 'needs step open_target in the same run'
    got = {}
    for gap in (0, 8):
        saved = with_gap(gap)
        try:
            port('WIN snap left')
            time.sleep(1)
            got[gap] = box()
        finally:
            restore_prefs(saved)
    ok = all(got.values()) and got[8][0] - got[0][0] == 8 and got[8][1] - got[0][1] == 8
    return ok, 'snap left at gap 0 %r, at gap 8 %r (left and top must differ by 8)' % (got.get(0), got.get(8))


ROWS = [
    ('start',          s_start),
    ('open_target',    s_open_target),
    ('move_right',     s_hotkey_move_right),
    ('menu_move_left', s_menu_move_left),
    ('move_left',      s_move_left),
    ('unknown_verb',   s_refuse_and_unknown),
    ('k6_front',       probe('front')),
    ('k6_back',        probe('back')),
    ('k6_zip',         probe('zip', 'zip')),
    ('k6_move',        probe('move down', 'move up')),
    ('k6_size',        probe('size right', 'size left')),
    ('k6_fullwidth',   probe('fullwidth', 'restore')),
    ('s2_screen_cycle', s_screen_cycle),
    ('bar_strut',      s_bar_strut),
    ('snap_status',    s_snap_status),
    ('snap_gap',       s_snap_gap),
    ('b2_redraw',      needs_wb(s_wb_redraw)),
    ('b2_patterns',    needs_wb(s_wb_patterns)),
    ('b2_icons_selected', needs_wb(s_wb_icons_selected)),
    ('b2_viewname',    needs_wb(s_wb_viewname)),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    stage_rig.report(stage_rig.stage_build(names=('Amarchy', 'amarchy-gui'), build=not a.no_build))
    passed = total = 0
    for name, fn in ROWS:
        f = OUT / (name + '.verdict')
        if a.only and a.only != name:
            continue
        if f.exists() and not a.fresh and not a.only:
            old = f.read_text()
            if old.startswith('ok') or not a.failed:
                total += 1
                passed += old.startswith('ok')
                continue
        try:
            ok, detail = fn()
        except Exception as e:                      # a step that cannot run is a failure, recorded
            ok, detail = False, 'exception %r' % (e,)
        f.write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
    print('amarchy_window_rig: passed %d of %d (verdicts in %s)' % (passed, total, OUT))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
