#!/usr/bin/env python3
"""amarchy_tile_rig.py -- reachability of AmTile, the autotiling commodity (plan 2026-10-28-amarchy-autotile.md,
Phase 4). Rig purpose text: "Amarchy s17: autotile reachability". Needs the rig up (UPTERM_RIG=<n> rig.py start --max
--purpose ...); this script never starts or stops a rig.

Entry point: the Amarchy menu row "Tile windows" (keymap row whose action runs `UP-Term:bin/AmTile TOGGLE`), fired
through port AMARCHY as `RUN <row index>` (the same dispatch a menu pick or hotkey reaches: amarchy.c rexx_cmd ->
do_row). Sentinel: AMTILE STATUS "on|off <tiled> <retiles>"; the retile count rises each time boxes are applied.
Expected boxes: tools/rig/amarchy_tile_boxes.c, built for the host from amarchy/tile_core.c + win_core.c (the same
code the commodity runs); the formula is not repeated here.

Steps, in order (each prints "[OK] <step>: ..." or "[FAIL] <step>: <expected> vs <got>"):
  start          Amarchy running, AmTile stopped, ENV:Amarchy/tile "off", two UP-Term windows open
  row_on         the "Tile windows" row turns tiling on (STATUS "on 2 n", n >= 1)
  boxes2         the two windows sit on the n = 2 tile_core boxes
  second_on      `AmTile ON` while running forwards, rc 0, still one instance
  env_on         ENV:Amarchy/tile reads "on"
  third          a third window opens: within 1 s it is the master, the others stack; retile count grows
  close_one      one window closed: the remaining two sit on the n = 2 boxes
  midapply       a window closed right after it opened: AmTile still answers STATUS
  off            the row (TOGGLE) turns tiling off, ENV:Amarchy/tile reads "off"
  fourth_free    a fourth window opened while off is not moved
  restart        ENV "on", Amarchy restarted: port AMTILE is there again
Layout steps (plan 2026-10-28-amtile-layout.md, Phase 5; STATUS "<on|off> <tiled> <retiles> <layout> <nmaster> <ratio>"):
  layout_default no tile-* prefs keys: STATUS dwindle 1 55
  layout_row     the dist/keymap row `AmTile LAYOUT NEXT` via RUN <n> (THE reachability test: layout changes, retiles up, boxes)
  layout_hotkey  super+ctrl+l through the keyboard (qualifier LAmiga+Ctrl, raw 0x28)
  lay_<name>     LAYOUT <name> with 3 windows: STATUS names it, boxes equal the host helper's
  lay_next_prev, ratio, nmaster, promote, cycle, monocle_raise, persist, reset   see the plan's Reachability section
Resumable like the other rigs: --only <step> reruns one step against the state the earlier steps left (the windows
this script opened are remembered in <RIG>/amarchy-tile/state.json).
  python3 tools/rig/amarchy_tile_rig.py [--only STEP] [--screen WxH] [--title N] [--gap N] [--ratio N] [--no-build]
Exit code is nonzero when any step fails."""
import argparse, json, pathlib, re, subprocess, sys, tempfile, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig

REPO = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(paths.RIG) / 'amarchy-tile'
KEYMAP = REPO / 'dist' / 'keymap'
ROW_MARK = 'AmTile TOGGLE'      # the Phase 3 row "Tile windows" runs UP-Term:bin/AmTile TOGGLE
ENV_FILE = 'ENV:Amarchy/tile'
WATCH = 0.25                    # AmTile's poll period (tile.c); a retile is expected within 1 s
TITLE_DEFAULT = 14             # rig evidence (rig 7); wasabi exposes no screen BarHeight
OPT = {'gap': 4, 'ratio': 55, 'layout': 'dwindle', 'nmaster': 1, 'title': None, 'screen': None}   # layout/nmaster/ratio: boot defaults (L1)
STATE = {'wins': []}            # addrs of the windows this script opened, oldest first
WIN_TITLE = 'UP-Term'           # title of every window ami.open_window opens
HELPER = []


# ---- ports --------------------------------------------------------------------------------------------------

def send(portname, cmd, timeout=15):
    """(rc, result) of one command to an ARexx port; (None, '') when there is no such port"""
    m = ami._m()
    try:
        with ami._conn() as conn:
            rc, _, res = m.arexx(conn, portname, cmd, timeout=timeout)
            return rc, res.strip()
    except m.WasabiError:
        return None, ''


def tile_status():
    """(on?, tiled, retiles, layout, nmaster, ratio) from AMTILE STATUS "<on|off> <tiled> <retiles> <layout> <nmaster>
    <ratio>", None when the port does not answer or the text is odd"""
    rc, res = send('AMTILE', 'STATUS', timeout=5)
    m = re.match(r'(on|off) (\d+) (\d+) (\S+) (\d+) (\d+)(?: \d+ \d+)?$', res)
    if rc != 0 or not m:
        return None
    return m.group(1) == 'on', int(m.group(2)), int(m.group(3)), m.group(4), int(m.group(5)), int(m.group(6))


def wait_for(pred, secs, step=0.2):
    end = time.time() + secs
    while True:
        v = pred()
        if v:
            return v
        if time.time() >= end:
            return v
        time.sleep(step)


# ---- the Amarchy row ----------------------------------------------------------------------------------------

def row_index(mark=None):
    """index of the keymap row whose line holds `mark` (default ROW_MARK, AmTile TOGGLE), counted the way Amarchy indexes `RUN <n>`: amarchy.c reload()
    loads the table with keymap_reload_compact(cx_row_installed), which DROPS the terminal-scope rows (tab:, menuid:,
    slash: = KS_TERMINAL in keymap_action_scope), so cx_row(n) and STATUS rows= count only the installed rows;
    "super+1..9" is nine rows. None when dist/keymap has no such row (Phase 3 not built)"""
    n, sect = 0, ''
    for l in KEYMAP.read_text(encoding='latin-1').splitlines():
        l = l.strip()
        if l.startswith('['):
            sect = l
        elif sect == '[bind]' and l and not l.startswith(';') and '=' in l:
            action = l.split('=', 1)[1].strip()
            if re.match(r'(tab|menuid|slash):', action):
                continue
            if (mark or ROW_MARK) in l:
                return n
            m = re.match(r'\S*?(\d)\.\.(\d)\s*=', l)
            n += int(m.group(2)) - int(m.group(1)) + 1 if m else 1
    return None


def fire_row(mark=None):
    i = row_index(mark)
    if i is None:
        return None, 'no keymap row runs "%s" (dist/keymap)' % (mark or ROW_MARK)
    return send('AMARCHY', 'RUN %d' % i)[0], 'RUN %d' % i


# ---- windows ------------------------------------------------------------------------------------------------

def front_screen():
    with ami._conn() as conn:
        s = ami._m().list_windows(conn)
    s = s[0] if isinstance(s, tuple) else s       # wasabi returns (screens, raw lines)
    return s[0] if s else None


def boxes():
    """{addr: (left, top, width, height, title)} of the front screen's windows, in uitree pixels (rows undoubled)"""
    sc = front_screen()
    if not sc:
        return {}
    k = sc['row_scale']
    return {w['addr']: (w['left'], w['top'] // k, w['width'], w['height'] // k, w['title']) for w in sc['windows']}


def mine():
    b = boxes()
    return [(a, b[a]) for a in STATE['wins'] if a in b]


def screen_geom():
    """(width, height, title bar height). Width and height from the front screen (or --screen); the title bar height is
    --title, else the master's top minus the gap measured from a live n = 1 layout is not possible without trusting the
    product, so the default is 14, the value measured on rig 7 (thoughts/shared/research/2026-10-28_amarchy-tile-rig7.md;
    wasabi's screen listing carries no BarHeight); --title overrides"""
    sc = front_screen()
    w, h = (OPT['screen'] or (sc['width'], sc['height']))
    return w, h, OPT['title'] if OPT['title'] is not None else TITLE_DEFAULT


def helper():
    if not HELPER:
        out = pathlib.Path(tempfile.mkdtemp(prefix='amarchy_tile_boxes')) / 'tile_boxes'
        r = subprocess.run(['cc', '-std=c99', '-o', str(out), 'tools/rig/amarchy_tile_boxes.c',
                            'amarchy/tile_core.c', 'amarchy/win_core.c'], cwd=REPO, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('amarchy_tile_boxes did not build:\n' + r.stderr[-600:])
        HELPER.append(str(out))
    return HELPER[0]


def expected(n, layout=None, nmaster=None, ratio=None):
    """the boxes of the n-window layout, newest (master) first; layout/nmaster/ratio default to OPT"""
    w, h, t = screen_geom()
    r = subprocess.run([helper(), str(w), str(h), str(t), str(OPT['gap']), str(ratio or OPT['ratio']), str(n),
                        layout or OPT['layout'], str(nmaster or OPT['nmaster'])] + (['50', '30', '0', '0', str(OPT['bottom'])] if OPT.get('bottom') else []),
                       capture_output=True, text=True, check=True)
    return [tuple(int(x) for x in l.split()[2:6]) for l in r.stdout.splitlines()]


def uterm_addrs():
    return {a for a, b in boxes().items() if b[4] == WIN_TITLE}


def open_win():
    """open one UP-Term window and return the address of the UP-Term window that was not there before. ami.open_window's
    own diff keys on (addr, task, title): a window whose task name changes after it opened (or a Workbench window) looks
    new, so its first entry can be an old window, and the real new one then leaked untracked (rig 7, run 2)"""
    before = uterm_addrs()
    if not ami.open_window():
        return None
    new = wait_for(lambda: uterm_addrs() - before, 10)
    if not new or len(new) != 1:
        return None
    addr = next(iter(new))
    STATE['wins'].append(addr)
    return addr


def close_win(addr):
    b = boxes().get(addr)
    if not b:
        return False
    with ami._conn() as conn:
        m = ami._m()
        # every window is titled "UP-Term": a click by title takes the first match, so select the window by its address
        for sc, w in m.ui_windows(m.read_ui(conn, every_screen=True), b[4]):
            if w['addr'] != addr:
                continue
            g = next((x for x in w['gadgets'] if x['kind'] == 'close'), None)
            if not g:
                return False
            ami._mouse(conn, 1, 0, 1, g['x'] + g['width'] // 2, g['y'] + g['height'] // 2)
            break
        else:
            return False
    gone = wait_for(lambda: addr not in boxes(), 3) is not None      # a click that closed nothing must not drop the window from STATE
    if gone and addr in STATE['wins']:
        STATE['wins'].remove(addr)
    return gone


def known_window_set():
    """every step starts from the windows the script tracks and no others: addresses that are gone leave STATE['wins'],
    and any other UP-Term window on the front screen (leaked by an earlier step or run, or opened by hand) is closed;
    returns the number closed"""
    STATE['wins'] = [a for a in STATE['wins'] if a in boxes()]
    closed = 0
    for _ in range(12):
        stray = [a for a, b in boxes().items() if b[4] == WIN_TITLE and a not in STATE['wins']]
        if not stray:
            break
        if not close_win(stray[0]):
            break
        closed += 1
    time.sleep(WATCH * 3)
    return closed


def tiled_boxes_match(n, secs=3, **kw):
    """True when the n windows of mine sit exactly on the n-window layout (any assignment of windows to cells would be a
    different layout, so the multiset of boxes is compared); kw: layout/nmaster/ratio as for expected(); (ok, want, got)"""
    want = sorted(expected(n, **kw))

    def got():
        return sorted(b[:4] for _, b in mine())
    wait_for(lambda: got() == want, secs)
    g = got()
    return g == want, want, g


# ---- layout helpers -----------------------------------------------------------------------------------------

LAYOUTS = ['dwindle', 'master-stack', 'mirrored', 'columns', 'rows', 'grid', 'monocle']
RAW_KEY = {'t': 0x14, 'l': 0x28}   # raw codes of the letters the hotkeys use (amarchy_clash_rig.py RAW: q=0x10.., a=0x20..)
SUPER_CTRL = 0x40 | 0x08           # IEQUALIFIER_LCOMMAND | IEQUALIFIER_CONTROL = 0x48 (LAmiga+LAlt is Intuition's left mouse button)


def ensure_windows(n):
    """open or close windows of mine (newest first out) until exactly n remain; AmTile (when on) retiles them"""
    while len(STATE['wins']) > n:
        if not close_win(STATE['wins'][-1]):
            break
    while len(STATE['wins']) < n:
        if not open_win():
            break
    time.sleep(WATCH * 3)
    return len(mine()) == n


def cmd(text):
    """one AMTILE command; the rc (None: no port)"""
    return send('AMTILE', text)[0]


def status_is(**want):
    """the STATUS tuple once its named fields (on, tiled, retiles, layout, nmaster, ratio) equal `want`, else None"""
    names = ('on', 'tiled', 'retiles', 'layout', 'nmaster', 'ratio')

    def ok():
        st = tile_status()
        return st if st and all(st[names.index(k)] == v for k, v in want.items()) else None
    return wait_for(ok, 3)


def hotkey(letter):
    """super+ctrl+<letter> through the keyboard path (ami.key = one KEY frame down+up)"""
    ami.key(RAW_KEY[letter], SUPER_CTRL)


def topmost():
    """address of the window of mine that Intuition's layer hit test puts on top. wasabi's window list is NOT layer order
    (the listing never moved after WindowToFront or CYCLE on rig 7; covered() on it is blind). A left click is delivered
    to, and activates, the topmost layer under the pointer (WhichLayer) and does not change the depth order, so the
    click at the centre of the stack followed by reading the active flag names the layer top. Needs the windows to
    overlap at that point (monocle, or any point inside the first window's box); None when no window of mine is active"""
    mine_now = mine()
    if not mine_now:
        return None
    _, b = mine_now[0]
    x, y = b[0] + b[2] // 2, b[1] + b[3] // 2
    with ami._conn() as conn:
        ami._mouse(conn, 1, 0, 1, x, y)
    time.sleep(0.3)
    sc = front_screen()
    act = [w['addr'] for w in sc['windows'] if w['active']] if sc else []
    return act[0] if len(act) == 1 and act[0] in STATE['wins'] else None


def activate(addr):
    """bring the window with this address to the front and make it active (ui_front, then a click in its middle)"""
    with ami._conn() as conn:
        m = ami._m()
        for sc, w in m.ui_windows(m.read_ui(conn, every_screen=True), boxes()[addr][4]):
            if w['addr'] == addr:
                m.ui_front(conn, w)
                ami._mouse(conn, 1, 0, 1, w['left'] + w['width'] // 2, w['top'] + w['height'] // 2)
                return True
    return False


def layout_boxes_ok(n, layout, **kw):
    ok, want, got = tiled_boxes_match(n, layout=layout, **kw)
    return ok, 'boxes %r vs %r' % (want, got)


# ---- steps --------------------------------------------------------------------------------------------------

def set_env(text):
    c.run('MakeDir ENV:Amarchy')
    c.run('Echo "%s" >%s' % (text, ENV_FILE))


def env_text():
    return c.run('Type ' + ENV_FILE)[1].strip()


def strip_tile_prefs():
    """no tile-layout/ratio/nmaster keys in the prefs: AmTile starts at its boot default (an earlier run's persisted layout
    otherwise changes the cells every later step expects)"""
    for d in ('ENV:', 'ENVARC:'):
        f = d + 'Amarchy/prefs'
        raw = ami.read_file(f)
        if raw is not None:
            keep = [l for l in raw.decode('latin-1').splitlines()
                    if not l.strip().startswith(('tile-layout', 'tile-ratio', 'tile-nmaster'))]
            with tempfile.NamedTemporaryFile('w', delete=False, encoding='latin-1') as t:
                t.write('\n'.join(keep) + '\n')
            ami.put(t.name, f)


def s_start():
    send('AMTILE', 'QUIT')
    send('AMARCHY', 'QUIT')
    time.sleep(1.5)
    strip_tile_prefs()
    set_env('off')
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    ok = wait_for(lambda: send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
    STATE['wins'] = []
    a, b = open_win(), open_win()
    time.sleep(1)
    return bool(ok and a and b and len(mine()) == 2 and tile_status() is None), \
        'Amarchy up %s, windows %r %r, AMTILE port %s' % (bool(ok), a, b, 'absent' if tile_status() is None else 'PRESENT')


def s_row_on():
    rc, how = fire_row()
    st = wait_for(lambda: tile_status() if (tile_status() or (0, 0, 0))[2] >= 1 else None, 5)
    ok = rc == 0 and st is not None and st[0] and st[1] == 2 and st[2] >= 1
    return ok, 'expected row rc 0 and STATUS "on 2 n" n>=1 vs %s rc %r STATUS %r' % (how, rc, st)


def s_boxes2():
    ok, want, got = tiled_boxes_match(2)
    return ok, 'expected %r vs %r' % (want, got)


def s_second_on():
    rc, _ = c.run('%s/AmTile ON' % paths.UPTERM_BIN)
    st = tile_status()
    ps = c.run('Status')[1]
    return rc == 0 and st is not None and st[0], 'expected rc 0, STATUS on vs rc %r STATUS %r (Status: %d lines)' % (
        rc, st, len(ps.splitlines()))


def s_env_on():
    got = env_text()
    return got == 'on', 'expected %s "on" vs %r' % (ENV_FILE, got)


def s_third():
    before = tile_status()
    addr = open_win()
    t0 = time.time()
    ok, want, got = False, None, None
    want = expected(3)
    while time.time() - t0 < 1.0:
        if sorted(b[:4] for _, b in mine()) == sorted(want):
            ok = True
            break
        time.sleep(0.1)
    took = time.time() - t0
    b = boxes().get(addr)
    master = b is not None and b[:4] == want[0]
    st = tile_status()
    grew = before is not None and st is not None and st[2] > before[2] and st[1] == 3
    return ok and master and grew, ('expected 3 windows on %r within 1 s, new window master %r, retiles > %r vs '
                                    'on cells %s in %.1f s, new window %r, STATUS %r' % (
                                        sorted(want), want[0], before, ok, took, b and b[:4], st))


def s_close_one():
    # a stack window, not the master: close the oldest of mine
    addr = STATE['wins'][0] if STATE['wins'] else None
    gone = addr is not None and close_win(addr)
    ok, want, got = tiled_boxes_match(2, 3)
    st = tile_status()
    return gone and ok and st is not None and st[1] == 2, 'expected 2 windows on %r, STATUS tiled 2 vs closed %s boxes %r STATUS %r' % (
        want, gone, got, st)


def s_midapply():
    addr = open_win()
    closed = close_win(addr) if addr else False
    time.sleep(1.5)
    st = tile_status()
    return closed and st is not None, 'expected AmTile answers STATUS after open+immediate close vs closed %s STATUS %r' % (closed, st)


def s_layout_default():
    """boot default L1: with no tile-* keys in the prefs AmTile starts dwindle 1 55"""
    send('AMTILE', 'QUIT')
    wait_for(lambda: tile_status() is None, 5)
    strip_tile_prefs()
    set_env('on')
    c.run('Run >NIL: <NIL: %s/AmTile ON' % paths.UPTERM_BIN)
    st = wait_for(tile_status, 10)
    return bool(st) and st[3:] == ('dwindle', 1, 55), 'expected STATUS "dwindle 1 55" without tile-* keys vs %r' % (st,)


def s_layout_row():
    """THE reachability test: the dist/keymap row `LAYOUT NEXT` fired through port AMARCHY changes the layout"""
    ensure_windows(2)
    cmd('LAYOUT dwindle')
    before = status_is(layout='dwindle', tiled=2)
    rc, how = fire_row('AmTile LAYOUT NEXT')
    after = status_is(layout='master-stack')
    grew = before is not None and after is not None and after[2] > before[2]
    ok, d = layout_boxes_ok(2, 'master-stack')
    return rc == 0 and grew and ok, ('expected before "dwindle", row rc 0, retiles up, layout master-stack, helper boxes vs %s rc %r '
                                     'before %r after %r %s' % (how, rc, before, after, d))


def s_layout_hotkey():
    """super+ctrl+l (LAYOUT NEXT) through the keyboard: master-stack -> mirrored, retile count up"""
    before = status_is(layout='master-stack')
    hotkey('l')
    after = status_is(layout='mirrored')
    grew = before is not None and after is not None and after[2] > before[2]
    ok, d = layout_boxes_ok(2, 'mirrored')
    return grew and ok, 'expected hotkey super+ctrl+l: master-stack -> mirrored, retiles up, helper boxes vs before %r after %r %s' % (
        before, after, d)


def lay_step(name):
    def step():
        ensure_windows(3)
        rc = cmd('LAYOUT ' + name)
        st = status_is(layout=name)
        ok, d = layout_boxes_ok(3, name)
        if name == 'monocle':
            ok = ok and len({b[:4] for _, b in mine()}) == 1       # all three on the whole work area
        return rc == 0 and st is not None and ok, 'expected LAYOUT %s rc 0, STATUS names it, 3 helper boxes vs rc %r STATUS %r %s' % (
            name, rc, st, d)
    return step


def s_lay_next_prev():
    cmd('LAYOUT dwindle')
    rc1 = cmd('LAYOUT NEXT')
    nxt = status_is(layout='master-stack')
    rc2 = cmd('LAYOUT PREV')
    prv = status_is(layout='dwindle')
    return rc1 == 0 and rc2 == 0 and nxt is not None and prv is not None, 'expected NEXT -> master-stack, PREV -> dwindle vs rc %r %r STATUS %r %r' % (
        rc1, rc2, nxt, prv)


def s_ratio():
    ensure_windows(3)
    cmd('LAYOUT dwindle')
    cmd('RATIO 55')
    rc_a, rc_b = cmd('RATIO 65'), cmd('RATIO -5')
    st = status_is(ratio=60)
    ok, d = layout_boxes_ok(3, 'dwindle', ratio=60)
    rc_bad = cmd('RATIO 90')
    still = status_is(ratio=60)
    return rc_a == 0 and rc_b == 0 and st is not None and ok and rc_bad == 10 and still is not None, (
        'expected RATIO 65 then -5 -> ratio 60 and dwindle(60) boxes, RATIO 90 rc 10 vs rcs %r %r STATUS %r %s, bad rc %r' % (
            rc_a, rc_b, st, d, rc_bad))


def s_nmaster():
    ensure_windows(4)
    cmd('RATIO 55')
    cmd('LAYOUT master-stack')
    rc = cmd('NMASTER 2')
    st = status_is(nmaster=2, layout='master-stack')
    ok, d = layout_boxes_ok(4, 'master-stack', nmaster=2)
    rc_bad = cmd('NMASTER 0')
    ensure_windows(3)
    return rc == 0 and st is not None and ok and rc_bad == 10, 'expected NMASTER 2 -> nmaster 2 and helper boxes, NMASTER 0 rc 10 vs rc %r STATUS %r %s bad rc %r' % (
        rc, st, d, rc_bad)


def s_promote():
    ensure_windows(3)
    cmd('NMASTER 1')
    cmd('LAYOUT master-stack')
    status_is(layout='master-stack', nmaster=1, tiled=3)
    tiled_boxes_match(3, layout='master-stack', nmaster=1)      # STATUS changes before the windows move: act on the settled cells
    target = STATE['wins'][1]      # STATE['wins'] is oldest first: the middle window of three is the second-newest
    old = boxes().get(target)
    act = activate(target)
    rc = cmd('PROMOTE')
    want = expected(3, layout='master-stack', nmaster=1)[0]
    got = wait_for(lambda: (lambda b: b if b and b[:4] == want else None)(boxes().get(target)), 3)
    return act and rc == 0 and got is not None, 'expected PROMOTE rc 0 and the activated window on the master box %r vs activated %s rc %r was %r now %r' % (
        want, act, rc, old and old[:4], got and got[:4])


def s_cycle():
    ensure_windows(3)
    cmd('LAYOUT monocle')
    status_is(layout='monocle', tiled=3)
    start, before = topmost(), tile_status()
    seen, rcs = [], []
    for _ in range(3):
        rcs.append(cmd('CYCLE NEXT'))
        wait_for(lambda: topmost() not in ([start] + seen[-1:]), 1.5)
        seen.append(topmost())
    rcs.append(cmd('CYCLE PREV'))
    back = wait_for(lambda: topmost() == seen[1] and seen[1], 1.5)
    after = tile_status()
    ok = (all(r == 0 for r in rcs) and None not in seen and len(set(seen)) == 3 and seen[2] == start and back == seen[1]
          and before is not None and after is not None and before[:2] == after[:2])
    return ok, ('expected CYCLE NEXT x3 -> three different front windows ending at the start, PREV -> the second, STATUS on/tiled '
                'unchanged vs rcs %r start %r front %r back %r STATUS %r -> %r' % (rcs, start, seen, back, before, after))


def s_monocle_raise():
    ensure_windows(3)
    cmd('LAYOUT monocle')
    status_is(layout='monocle', tiled=3)
    res = []
    for w in (STATE['wins'][0], STATE['wins'][1]):       # window A, then window B
        activate(w)
        res.append(bool(wait_for(lambda: topmost() == w, 1.5)))
    return all(res), 'expected the activated window front-most within 1 s each time (A then B) vs %r' % (res,)


def s_persist():
    ensure_windows(3)
    rcs = [cmd('LAYOUT grid'), cmd('RATIO 60'), cmd('NMASTER 2')]
    send('AMTILE', 'QUIT')
    gone = wait_for(lambda: tile_status() is None, 5)
    c.run('Run >NIL: <NIL: %s/AmTile ON' % paths.UPTERM_BIN)
    st = wait_for(tile_status, 10)
    txt = (ami.read_file('ENV:Amarchy/prefs') or b'').decode('latin-1')
    keys = all(re.search(r'^%s\s*=?\s*%s\s*$' % (k, v), txt, re.M)
               for k, v in (('tile-layout', 'grid'), ('tile-ratio', '60'), ('tile-nmaster', '2')))
    return all(r == 0 for r in rcs) and gone and bool(st) and st[3:] == ('grid', 2, 60) and keys, (
        'expected STATUS "grid 2 60" after QUIT + fresh AmTile ON and the three keys in ENV:Amarchy/prefs vs rcs %r gone %s STATUS %r keys %s' % (
            rcs, bool(gone), st, keys))


def s_reset():
    rcs = [cmd('LAYOUT dwindle'), cmd('RATIO 55'), cmd('NMASTER 1')]
    st = status_is(layout='dwindle', ratio=55, nmaster=1)
    return all(r == 0 for r in rcs) and st is not None, 'expected the rig image back at dwindle 1 55 vs rcs %r STATUS %r' % (rcs, st)


def s_off():
    rc, how = fire_row()
    st = wait_for(lambda: (lambda s: s if s and not s[0] else None)(tile_status()), 5)
    env = env_text()
    return rc == 0 and st is not None and env == 'off', 'expected row rc 0, STATUS off, %s "off" vs %s rc %r STATUS %r env %r' % (
        ENV_FILE, how, rc, st, env)


def s_fourth_free():
    addr = open_win()
    if not addr:
        return False, 'expected a fourth window vs none opened'
    b0 = boxes().get(addr)
    time.sleep(1.5)
    b1 = boxes().get(addr)
    cells = [x for x in expected(len(mine())) if b1 and x == b1[:4]]
    return b0 is not None and b0 == b1 and not cells, 'expected box unchanged and not a layout cell vs %r -> %r' % (b0, b1)


def s_restart():
    set_env('on')
    send('AMTILE', 'QUIT')
    wait_for(lambda: tile_status() is None, 5)
    send('AMARCHY', 'QUIT')
    time.sleep(1.5)
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    st = wait_for(tile_status, 10)
    return st is not None and st[0], 'expected port AMTILE present, on, after Amarchy restart with %s "on" vs STATUS %r' % (
        ENV_FILE, st)


def tile_floats():
    """field 7 of AMTILE STATUS (windows floated by FLOAT, P3-C2), None when absent"""
    rc, res = send('AMTILE', 'STATUS', timeout=5)
    f = res.split()
    return int(f[6]) if rc == 0 and len(f) > 6 and f[6].isdigit() else None


def tile_swaps():
    """field 8 of AMTILE STATUS (SWAP count, P3-C3), None when absent"""
    rc, res = send('AMTILE', 'STATUS', timeout=5)
    f = res.split()
    return int(f[7]) if rc == 0 and len(f) > 7 and f[7].isdigit() else None


def hexid(addr):
    return '%08x' % int(str(addr), 0) if not isinstance(addr, int) else '%08x' % addr


def s_float_toggle():
    """P3-C2: FLOAT ON <id> takes one of 3 tiled windows out (tiled 3 -> 2, floats +1, the other two on the 2-window
    layout), FLOAT OFF puts it back, a bogus id answers rc 5, a bad word rc 10"""
    if not ensure_windows(3) or not status_is(on=True):
        return False, 'needs AmTile on with 3 windows of mine'
    cmd('LAYOUT dwindle')
    st0 = status_is(tiled=3)
    f0 = tile_floats()
    addr = STATE['wins'][0]
    rc_on, _ = send('AMTILE', 'FLOAT ON ' + hexid(addr))
    st1 = status_is(tiled=2)
    f1 = tile_floats()
    rest = sorted(b[:4] for a, b in mine() if a != addr)
    ok2 = sorted(expected(2)) == rest
    rc_off, _ = send('AMTILE', 'FLOAT OFF ' + hexid(addr))
    st2 = status_is(tiled=3)
    rc_bad, _ = send('AMTILE', 'FLOAT ON 00000001')
    rc_word, _ = send('AMTILE', 'FLOAT MAYBE')
    ok = (st0 and rc_on == 0 and st1 and f0 is not None and f1 == f0 + 1 and ok2 and rc_off == 0 and st2 and
          tile_floats() == f0 and rc_bad == 5 and rc_word == 10)
    return bool(ok), 'rc on %r tiled3 %r tiled2 %r floats %r -> %r, rest on 2-layout %s, rc off %r back to 3 %r, bogus id rc %r (5), bad word rc %r (10)' % (
        rc_on, bool(st0), bool(st1), f0, f1, ok2, rc_off, bool(st2), rc_bad, rc_word)


def s_swap_right():
    """P3-C3: master-stack, 2 windows: the active master and the stack window exchange boxes after SWAP RIGHT; SWAP LEFT
    back; the swap counter rises; SWAP UP on the master answers rc 5 (nothing above)"""
    if not ensure_windows(2) or not status_is(on=True):
        return False, 'needs AmTile on with 2 windows of mine'
    cmd('LAYOUT master-stack')
    status_is(layout='master-stack', tiled=2)
    want = expected(2, layout='master-stack')
    m_box, s_box = want[0], want[1]
    owner = {a: b[:4] for a, b in mine()}
    master = next((a for a, b in owner.items() if b == m_box), None)
    if master is None:
        return False, 'no window on the master box %r in %r' % (m_box, owner)
    s0 = tile_swaps()
    activate(master)
    rc_r, _ = send('AMTILE', 'SWAP RIGHT')
    wait_for(lambda: {a: b[:4] for a, b in mine()}.get(master) == s_box, 3)
    after = {a: b[:4] for a, b in mine()}
    swapped = after.get(master) == s_box and sorted(after.values()) == sorted(owner.values()) and after != owner
    s1 = tile_swaps()
    activate(master)
    rc_l, _ = send('AMTILE', 'SWAP LEFT')
    wait_for(lambda: {a: b[:4] for a, b in mine()}.get(master) == m_box, 3)
    back = {a: b[:4] for a, b in mine()} == owner
    activate(master)
    rc_u, _ = send('AMTILE', 'SWAP UP')
    cmd('LAYOUT dwindle')
    ok = rc_r == 0 and swapped and s0 is not None and s1 == s0 + 1 and rc_l == 0 and back and rc_u == 5
    return ok, 'SWAP RIGHT rc %r swapped %s swaps %r -> %r, LEFT rc %r back %s, UP rc %r (5)' % (rc_r, swapped, s0, s1, rc_l, back, rc_u)


CONF = 'ENV:Amarchy/tile.conf'


def s_optout():
    """P3-C4: tile.conf with a pattern that matches no window title leaves 3 tiled; "up-t#?" (case-insensitive) matches the
    rig windows' title "UP-Term" so none tile after RETILE; removing the file and RETILE tiles them again"""
    if not ensure_windows(3) or not status_is(on=True):
        return False, 'needs AmTile on with 3 windows of mine'
    c.run('MakeDir ENV:Amarchy')
    c.run('Echo "; rig\nNoSuchWindow#?" >' + CONF)
    rc1 = cmd('RETILE')
    keep = status_is(tiled=3)
    c.run('Echo "up-t#?" >' + CONF)
    rc2 = cmd('RETILE')
    gone = status_is(tiled=0)
    c.run('Delete %s QUIET' % CONF)
    rc3 = cmd('RETILE')
    back = status_is(tiled=3)
    return all(r == 0 for r in (rc1, rc2, rc3)) and bool(keep and gone and back), (
        'expected tiled 3 with a non-matching pattern, 0 with "up-t#?", 3 with no file vs %s %s %s (rcs %r)' % (
            bool(keep), bool(gone), bool(back), (rc1, rc2, rc3)))


def bar_box():
    return next((b for b in boxes().values() if b[4] == 'Amarchy bar'), None)


def s_tile_bar():
    """P3-C1: with the Amarchy bar on the screen the tiled boxes equal the helper's boxes with the bottom reserve (the
    bar's height). FAILs when no window titled "Amarchy bar" is on the front screen"""
    bar = bar_box()
    if bar is None:
        return False, 'needs the Amarchy bar window on the front screen (none listed)'
    if not ensure_windows(3) or not status_is(on=True):
        return False, 'needs AmTile on with 3 windows of mine'
    OPT['bottom'] = bar[3]
    try:
        ok, want, got = tiled_boxes_match(3)
    finally:
        OPT.pop('bottom', None)
    return ok, 'bar height %d, boxes %r vs %r' % (bar[3], want, got)


def s_snap_floats():
    """P3-B4: three tiled windows, AMARCHY WIN snap left on one: AmTile floats +1 (the other two re-tile) and the
    snapped box stays after 1 s. Needs AmTile's FLOAT command (P3-C2); without it the floats field is absent: FAIL."""
    if not ensure_windows(3) or not status_is(on=True)[:1]:
        return False, 'needs AmTile on with 3 windows of mine'
    f0 = tile_floats()
    addr = STATE['wins'][-1]
    activate(addr)
    rc, _ = send('AMARCHY', 'WIN snap left')
    time.sleep(1)
    b0 = boxes().get(addr)
    time.sleep(1)
    b1, f1 = boxes().get(addr), tile_floats()
    ok = rc == 0 and f0 is not None and f1 == f0 + 1 and b0 is not None and b0 == b1
    return ok, 'snap rc %r, floats %r -> %r, snapped box %r then %r' % (rc, f0, f1, b0, b1)


STEPS = [('start', s_start), ('row_on', s_row_on), ('boxes2', s_boxes2), ('second_on', s_second_on),
         ('env_on', s_env_on), ('third', s_third), ('close_one', s_close_one), ('midapply', s_midapply),
         ('layout_default', s_layout_default), ('layout_row', s_layout_row), ('layout_hotkey', s_layout_hotkey)]
STEPS += [('lay_' + n.split('-')[0], lay_step(n)) for n in LAYOUTS]
STEPS += [('lay_next_prev', s_lay_next_prev), ('ratio', s_ratio), ('nmaster', s_nmaster), ('promote', s_promote),
          ('cycle', s_cycle), ('monocle_raise', s_monocle_raise), ('persist', s_persist), ('reset', s_reset),
          ('off', s_off), ('fourth_free', s_fourth_free), ('restart', s_restart), ('snap_floats', s_snap_floats),
          ('float_toggle', s_float_toggle), ('swap_right', s_swap_right), ('optout', s_optout), ('tile_bar', s_tile_bar)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', choices=[n for n, _ in STEPS], help='run one step against the state left by earlier runs')
    ap.add_argument('--screen', help='WxH of the UP-Term screen when the rig reports it wrongly')
    ap.add_argument('--title', type=int, help='screen title bar height (win_screen.title = BarHeight + 1), default %d' % TITLE_DEFAULT)
    ap.add_argument('--gap', type=int, default=4, help='tile_gap (default 4)')
    ap.add_argument('--ratio', type=int, default=55, help='master width percent (default 55)')
    ap.add_argument('--no-build', action='store_true', help='do not rebuild/stage Amarchy and AmTile')
    a = ap.parse_args()
    OPT.update(gap=a.gap, ratio=a.ratio, title=a.title,
               screen=tuple(int(x) for x in a.screen.lower().split('x')) if a.screen else None)
    OUT.mkdir(parents=True, exist_ok=True)
    state = OUT / 'state.json'
    if a.only and state.exists():
        STATE.update(json.loads(state.read_text()))
    stage_rig.report(stage_rig.stage_build(names=('Amarchy', 'AmTile'), build=not a.no_build))
    fails = 0
    for name, fn in STEPS:
        if a.only and a.only != name:
            continue
        try:
            known_window_set()
            ok, detail = fn()
        except Exception as e:               # a step that cannot run is a failure
            ok, detail = False, 'exception %r' % (e,)
        fails += not ok
        print('%s %s: %s' % ('[OK]' if ok else '[FAIL]', name, detail), flush=True)
        state.write_text(json.dumps(STATE))
    print('amarchy_tile_rig: %d step(s) failed' % fails)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
