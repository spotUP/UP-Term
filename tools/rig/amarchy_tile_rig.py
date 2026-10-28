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
OPT = {'gap': 4, 'ratio': 55, 'title': None, 'screen': None}
STATE = {'wins': []}            # addrs of the windows this script opened, oldest first
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
    """(on?, tiled, retiles) from AMTILE STATUS, None when the port does not answer or the text is odd"""
    rc, res = send('AMTILE', 'STATUS', timeout=5)
    m = re.match(r'(on|off) (\d+) (\d+)$', res)
    if rc != 0 or not m:
        return None
    return m.group(1) == 'on', int(m.group(2)), int(m.group(3))


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

def row_index():
    """index of the keymap row that runs AmTile TOGGLE, counted the way menu_rig.bind_rows counts the [bind] lines
    ("super+1..9" is nine rows); None when dist/keymap has no such row (Phase 3 not built)"""
    n, sect = 0, ''
    for l in KEYMAP.read_text(encoding='latin-1').splitlines():
        l = l.strip()
        if l.startswith('['):
            sect = l
        elif sect == '[bind]' and l and not l.startswith(';') and '=' in l:
            if ROW_MARK in l:
                return n
            m = re.match(r'\S*?(\d)\.\.(\d)\s*=', l)
            n += int(m.group(2)) - int(m.group(1)) + 1 if m else 1
    return None


def fire_row():
    i = row_index()
    if i is None:
        return None, 'no keymap row runs "%s" (dist/keymap)' % ROW_MARK
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
    product, so the default is the Workbench-style bar of the UP-Term screen: font height + 3 (topaz 8: 11)"""
    sc = front_screen()
    w, h = (OPT['screen'] or (sc['width'], sc['height']))
    return w, h, OPT['title'] if OPT['title'] is not None else 11


def helper():
    if not HELPER:
        out = pathlib.Path(tempfile.mkdtemp(prefix='amarchy_tile_boxes')) / 'tile_boxes'
        r = subprocess.run(['cc', '-std=c99', '-o', str(out), 'tools/rig/amarchy_tile_boxes.c',
                            'amarchy/tile_core.c', 'amarchy/win_core.c'], cwd=REPO, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('amarchy_tile_boxes did not build:\n' + r.stderr[-600:])
        HELPER.append(str(out))
    return HELPER[0]


def expected(n):
    w, h, t = screen_geom()
    r = subprocess.run([helper(), str(w), str(h), str(t), str(OPT['gap']), str(OPT['ratio']), str(n)],
                       capture_output=True, text=True, check=True)
    return [tuple(int(x) for x in l.split()[2:6]) for l in r.stdout.splitlines()]


def open_win():
    new = ami.open_window()
    if not new:
        return None
    addr = new[0]['addr']
    STATE['wins'].append(addr)
    return addr


def close_win(addr):
    b = boxes().get(addr)
    if not b:
        return False
    with ami._conn() as conn:
        ami._click_gadget(conn, b[4], 'close')
    if addr in STATE['wins']:
        STATE['wins'].remove(addr)
    return True


def tiled_boxes_match(n, secs=3):
    """True when the n windows of mine sit exactly on the n-window layout (any assignment of windows to cells would be a
    different layout, so the multiset of boxes is compared); (ok, want, got)"""
    want = sorted(expected(n))

    def got():
        return sorted(b[:4] for _, b in mine())
    wait_for(lambda: got() == want, secs)
    g = got()
    return g == want, want, g


# ---- steps --------------------------------------------------------------------------------------------------

def set_env(text):
    c.run('MakeDir ENV:Amarchy')
    c.run('Echo "%s" >%s' % (text, ENV_FILE))


def env_text():
    return c.run('Type ' + ENV_FILE)[1].strip()


def s_start():
    send('AMTILE', 'QUIT')
    send('AMARCHY', 'QUIT')
    time.sleep(1.5)
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


STEPS = [('start', s_start), ('row_on', s_row_on), ('boxes2', s_boxes2), ('second_on', s_second_on),
         ('env_on', s_env_on), ('third', s_third), ('close_one', s_close_one), ('midapply', s_midapply),
         ('off', s_off), ('fourth_free', s_fourth_free), ('restart', s_restart)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', choices=[n for n, _ in STEPS], help='run one step against the state left by earlier runs')
    ap.add_argument('--screen', help='WxH of the UP-Term screen when the rig reports it wrongly')
    ap.add_argument('--title', type=int, help='screen title bar height (win_screen.title = BarHeight + 1), default 11')
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
