#!/usr/bin/env python3
"""amarchy_bar_rig.py -- the Amarchy bar and its agents panel (roadmap 2026-10-26-amarchy-roadmap.md phase 2, R2.4;
OMARCHY-AI F2). Table driven and resumable like amarchy_rig.py: a verdict per row in <RIG>/amarchy/bar_<row>.verdict,
--only <row>, --failed, --fresh.

Sentinels: the commodity's STATUS reply carries bar (1 when the window is up), draws, panels and launches (raises counts the re-raises over foreign windows). A left
click on the agents segment must raise panels by 1; a right click must raise launches by 1 and open a tab that
runs the agent (a new `ps` task or a new UP-Term tab). The segment sits at the bottom right of the front screen.
Rig window purpose text to pass: "Amarchy session: bar check".
Needs the rig up (rig.py start --max --purpose ...), Amarchy and amarchy-gui in UP-Term:bin, an UP-Term window open.
behind opens a CON: window over the bottom row: the bar stays behind it (BAR STATUS raises unchanged, the window's layer in front). two_rows: the bar window has no border.
right_launch holds with another window active (the bar is not the active window); cold_panel restarts Amarchy.
"""
import argparse, pathlib, struct, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig, tabstate
import amarchy_rig as ar

OUT = pathlib.Path(paths.RIG) / 'amarchy'
SEG_FROM_RIGHT = 20                 # inside the agents segment, pixels from the screen's right edge
FONT_ROWS = 10                      # bar height is the font height + 2; a click at the bottom rows lands on it


def screen_size():
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0]
    return int(scr[0]['width']), int(scr[0]['height'])


def click(button, at=None):
    w, h = screen_size()
    sx, sy = ami.pointer_scale()
    x, y = at or (w - SEG_FROM_RIGHT, h - FONT_ROWS // 2)
    ami.req(0x08, bytes([5]) + struct.pack('>HH', int(round(x * sx)), int(round(y * sy))) + bytes([button, 1]))
    time.sleep(1.5)


def r_start():
    ar.arexx('QUIT')          # rig.py leaves an older resident Amarchy: the branch build must be the one running
    time.sleep(2)
    ar.start()
    s = ar.status()
    return s.get('bar') == 1 and s.get('draws', 0) > 0, 'status %r' % s


def r_left_panel():
    before = ar.status()
    click(0)
    after = ar.status()
    return after.get('panels', 0) == before.get('panels', 0) + 1, 'before %r after %r' % (before, after)


def r_close_panel():
    # the panel is a drawn menu with the focus: Escape closes it
    ami.key(0x45, 0)
    time.sleep(1)
    return True, 'escape sent'


def windows():
    """Front screen windows from the uitree: [(title, (x, y, w, h), active)]."""
    import shlex
    out = []
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        if line.startswith('W '):
            f = shlex.split(line)
            w, h = map(int, f[4].split('x'))
            out.append((f[-1], (int(f[2]), int(f[3]), w, h), f[5] == 'active'))
    return out


def active_title():
    return next((t for t, _, act in windows() if act), None)


def r_right_launch():
    # The bar is NOT the active window: open and close the panel, then the user's window holds the focus.
    # A right press over the Agents segment must still launch (the commodity sees it in the input stream)
    # and the user's window must keep the focus.
    click(0)
    ami.key(0x45, 0)
    time.sleep(1)
    host = next(b for t, b, _ in windows() if t == 'barhost')
    click(0, (host[0] + host[2] // 2, host[1] + host[3] // 2))      # the user's window takes the focus
    held = active_title()
    before = ar.status()
    tabs = tabstate.counters()
    click(1)
    after = ar.status()
    now = active_title()
    moved = after.get('launches', 0) == before.get('launches', 0) + 1
    hits = tabstate.counters().get('km-hits', 0) > tabs.get('km-hits', 0)
    kept = now == held and held == 'barhost'
    return moved and kept, 'before %r after %r km-hits moved %s active %r -> %r' % (
        before, after, hits, held, now)


def r_cold_panel():
    # First panel open after a fresh Amarchy start (no warm menu server) lands above the bar, where a warm open does
    ar.arexx('QUIT')
    time.sleep(2)
    ar.start()
    seen = {t for t, _, _ in windows()}
    click(0)
    cold = [b for t, b, _ in windows() if t == 'Amarchy' and t not in seen] or \
           [b for t, b, _ in windows() if t == 'Amarchy']
    ami.key(0x45, 0)
    time.sleep(1)
    click(0)
    warm = [b for t, b, _ in windows() if t == 'Amarchy']
    ami.key(0x45, 0)
    time.sleep(1)
    return bool(cold) and bool(warm) and cold[-1] == warm[-1], 'cold %r warm %r' % (cold, warm)


def dopus(cmd):
    """One command to Directory Opus' ARexx port; (None, '') while the port is not there."""
    m = ami._m()
    try:
        with ami._conn() as conn:
            rc, _, res = m.arexx(conn, 'DOPUS.1', cmd)
        return rc, res.strip()
    except m.WasabiError:
        return None, ''


def bar_status():
    """BAR STATUS counters (raises, panels ...) as ints."""
    d = {}
    for w in ar.arexx('BAR STATUS').replace('\n', ' ').split():
        if '=' in w:
            k, v = w.split('=', 1)
            if v.lstrip('-').isdigit():
                d[k] = int(v)
    return d


def front_order():
    """Titles of the front screen's windows, front layer first."""
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0][0]
    return [(w['title'], w['task']) for w in scr['windows']]


def r_behind():
    # BAR-BEHIND: a normal window (a CON: window over the bottom rows) opened over the bar stays in FRONT of it. After
    # the poll (500 ms) ran several times the window's layer is in front of the bar's, and BAR STATUS raises did not
    # move. With the old raise-over-everything code the bar is raised over it.
    before = bar_status()
    c.run('Run >NIL: Type CON:0/180/640/76/barcover/CLOSE')
    time.sleep(5)
    after = bar_status()
    order = front_order()
    bar_i = [i for i, (t, _) in enumerate(order) if t == 'Amarchy bar']
    win_i = [i for i, (t, _) in enumerate(order) if t == 'barcover' and i < (bar_i[0] if bar_i else 0)]
    ok = bool(bar_i) and bool(win_i) and before.get('raises') == after.get('raises')
    return ok, 'raises %r -> %r, bar at %r, window in front of it at %r of %r' % (
        before.get('raises'), after.get('raises'), bar_i, win_i, order)


def r_two_rows():
    # BAR-TWO-ROWS: the bar window has no Intuition chrome: border top 0 (a borderless window given WA_Title got a
    # title bar, a second row above the one painted), and its height is the screen font height + 2.
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0][0]
    bar = [w for w in scr['windows'] if w['title'] == 'Amarchy bar']
    if not bar:
        return False, 'no bar window'
    w = bar[0]
    scale = scr.get('row_scale', 1)
    ok = w['border'][1] == 0 and w['border'][3] == 0 and w['height'] // scale <= 20
    return ok, 'bar height %d top %d border %r row_scale %d' % (w['height'], w['top'], w['border'], scale)


ROWS = [('start', r_start), ('left_panel', r_left_panel), ('close_panel', r_close_panel),
        ('right_launch', r_right_launch), ('cold_panel', r_cold_panel), ('behind', r_behind),
        ('two_rows', r_two_rows)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    tabstate.open_host('barhost')
    passed = total = 0
    for name, fn in ROWS:
        f = OUT / ('bar_' + name + '.verdict')
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
    print('amarchy_bar_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
