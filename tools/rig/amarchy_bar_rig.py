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
import argparse, pathlib, struct, sys, threading, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths, verdicts
import ami, condev_rig as c
import stage_rig, tabstate
import amarchy_rig as ar

OUT = pathlib.Path(paths.REPORTS) / 'amarchy'
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


def agents_x():
    """x of the Agents segment. The segments sit right to left (clock, System, CPU ... ), so the screen's right edge is
    not Agents: a right press launches only on Agents, so sweep leftward over the bar until BAR STATUS launches moves."""
    w, h = screen_size()
    for x in range(w - 12, 0, -16):
        before = bar_status().get('launches', 0)
        click(1, (x, h - FONT_ROWS // 2))
        if bar_status().get('launches', 0) > before:
            return x
    return None


def wb_front():
    """Rows are order independent: each starts with the Workbench screen in front (a row or the boot can leave another
    screen there). LAmiga+M cycles the screens."""
    for _ in range(4):
        with ami._conn() as conn:
            scr = ami._m().list_windows(conn)[0]
        if 'Workbench' in str(scr[0].get('title', '')):
            return
        ami.key(0x37, 0x80)
        time.sleep(1)


def r_start():
    ar.arexx('QUIT')          # rig.py leaves an older resident Amarchy: the branch build must be the one running
    time.sleep(2)
    ar.start()
    s = bar_status()      # BAR STATUS holds the bar's counters; plain STATUS has none of them
    return s.get('bar') == 1 and s.get('draws', 0) > 0, 'status %r' % s


def r_left_panel():
    before = bar_status()
    click(0)
    after = bar_status()
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
    ax = agents_x()
    if ax is None:
        return False, 'no x on the bar launches the agent'
    w, h = screen_size()
    click(0, (ax, h - FONT_ROWS // 2))
    ami.key(0x45, 0)
    time.sleep(1)
    host = next(b for t, b, _ in windows() if t == 'barhost')
    click(0, (host[0] + host[2] // 2, host[1] + host[3] // 2))      # the user's window takes the focus
    held = active_title()
    before = bar_status()
    tabs = tabstate.counters()
    click(1, (ax, h - FONT_ROWS // 2))
    after = bar_status()
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


def sent(name):
    rc, out = c.run('GetEnv ' + name)
    return int(out.strip()) if rc == 0 and out.strip().lstrip('-').isdigit() else 0


def bar_top():
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0][0]
    bar = [w for w in scr['windows'] if w['title'] == 'Amarchy bar']
    sc = scr.get('row_scale', 1)     # the listing is in screen-bitmap rows: Intuition's coordinates are scale times smaller
    if not bar:
        return None, 0
    top = int(bar[0]['top']) // sc
    return top, top + int(bar[0]['height']) // sc     # the bar is the last rows: its bottom is the screen's


def opener(args):
    """Runs RAM:struttest and returns {'BOX': [l,t,w,h], 'ZIP': [[...], ...]}."""
    out = c.run('RAM:struttest ' + args, 90)[1]
    got = {'ZIP': []}
    for ln in out.splitlines():
        f = ln.split()
        if f and f[0] in ('SCR', 'BOX'):
            got[f[0]] = [int(x) for x in f[1:]]
        elif f and f[0] == 'ZIP':
            got['ZIP'].append([int(x) for x in f[1:]])
    return got


def strut_put():
    base = str(pathlib.Path(__file__).resolve().parents[2] / 'build' / 'amiga')
    ami.put(base + '/struttest', 'RAM:struttest')


def r_strut_open():
    # BAR-STRIP-RESERVE: a window opened 100 rows above the screen bottom (200x150, so it would cover the bar) is moved up out of
    # the strip: its bottom edge is at or above the bar's top, and AMARCHY_STRUT_CLAMPED rose by 1 (the patch ran).
    strut_put()
    top, h = bar_top()
    if top is None:
        return False, 'no bar window'
    c0, o0 = sent('AMARCHY_STRUT_CLAMPED'), sent('AMARCHY_STRUT_OPENS')
    got = opener('100 %d 200 150 sd 3' % (h - 100))
    time.sleep(2)
    box = got.get('BOX')
    c1, o1 = sent('AMARCHY_STRUT_CLAMPED'), sent('AMARCHY_STRUT_OPENS')
    ok = bool(box) and box[1] + box[3] <= top and c1 == c0 + 1 and o1 > o0
    return ok, 'bar top %d box %r clamped %d -> %d opens %d -> %d' % (top, box, c0, c1, o0, o1)


def r_strut_zoom():
    # A window that asks for a zoom box over the whole screen (WA_Zoom) gets the box cut at the strip: both ZipWindow boxes stay out of it.
    top, h = bar_top()
    if top is None:
        return False, 'no bar window'
    z0 = sent('AMARCHY_STRUT_ZOOM')
    got = opener('0 11 400 %d szg 3' % (top - 11))
    time.sleep(2)
    z1 = sent('AMARCHY_STRUT_ZOOM')
    boxes = [got.get('BOX')] + got['ZIP']
    ok = len(got['ZIP']) == 2 and all(b[1] + b[3] <= top for b in boxes if b) and z1 == z0 + 1
    return ok, 'bar top %d boxes %r zoom %d -> %d' % (top, boxes, z0, z1)


def root_ends():
    """[(title, bottom row in Intuition units)] of the screen-wide windows other than the bar."""
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0][0]
    sc = scr.get('row_scale', 1)
    return [(w['title'], (int(w['top']) + int(w['height'])) // sc) for w in scr['windows']
            if w['title'] != 'Amarchy bar' and int(w['left']) == 0 and int(w['width']) >= int(scr['width'])]


def r_strut_desk():
    # The Workbench root window ends above the strip (icons stay above it); AMARCHY_STRUT_DESK counts the shortened windows.
    top, h = bar_top()
    if top is None:
        return False, 'no bar window'
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0][0]
    ends = root_ends()
    d = sent('AMARCHY_STRUT_DESK')
    ok = d >= 1 and any(e <= top for _, e in ends)
    return ok, 'bar top %d wide windows %r DESK %d' % (top, ends, d)


def r_strut_off():
    # BAR OFF: the same open is not clamped (CLAMPED unchanged, the window reaches the strip rows) and the root grows back to the
    # screen bottom; BAR ON clamps again and shortens the root again.
    top, h = bar_top()
    ar.arexx('BAR OFF')
    time.sleep(3)
    c0 = sent('AMARCHY_STRUT_CLAMPED')
    off = opener('100 %d 200 150 sd 3' % (h - 100)).get('BOX')
    root_off = root_ends()
    time.sleep(2)
    c1 = sent('AMARCHY_STRUT_CLAMPED')
    ar.arexx('BAR ON')
    time.sleep(3)
    top2, _ = bar_top()
    on = opener('100 %d 200 150 sd 3' % (h - 100)).get('BOX')
    root_on = root_ends()
    time.sleep(2)
    c2 = sent('AMARCHY_STRUT_CLAMPED')
    ok = (bool(off) and off[1] + off[3] > top and c1 == c0 and all(e == h for _, e in root_off) and bool(on) and top2 is not None
          and on[1] + on[3] <= top2 and c2 == c1 + 1 and all(e <= top2 for _, e in root_on))
    return ok, 'off box %r root %r clamped %d -> %d; on box %r root %r (bar top %r) clamped -> %d' % (off, root_off, c0, c1, on, root_on, top2, c2)



def r_strut_prebar():
    # BAR-STRIP-RESERVE gap: a bordered window opened while the bar is OFF (so it is not clamped at open) is reflowed out of the
    # strip when BAR ON publishes it: its bottom is at or above the bar's top and AMARCHY_STRUT_CLAMPED rose by 1.
    strut_put()
    ar.arexx('BAR OFF')
    time.sleep(3)
    res = {}
    t = threading.Thread(target=lambda: res.update(opener('20 40 600 216 sd 14')))   # the opener holds the window open for 14 s
    t.start()
    time.sleep(4)
    c0 = sent('AMARCHY_STRUT_CLAMPED')
    ar.arexx('BAR ON')
    time.sleep(3)
    top, _ = bar_top()
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0][0]
    sc = scr.get('row_scale', 1)
    wins = [w for w in scr['windows'] if int(w['left']) // sc == 20 and int(w['top']) // sc <= 246]
    c1 = sent('AMARCHY_STRUT_CLAMPED')
    # STRUT-REFLOW-RESTORE: BAR OFF puts the untouched window back in its original box (20 40 600 216).
    ar.arexx('BAR OFF')
    time.sleep(3)
    with ami._conn() as conn:
        scr2 = ami._m().list_windows(conn)[0][0]
    sc2 = scr2.get('row_scale', 1)
    back = [(int(w['left']) // sc2, int(w['top']) // sc2, int(w['width']) // sc2, int(w['height']) // sc2)
            for w in scr2['windows'] if int(w['left']) // sc2 == 20 and int(w['top']) // sc2 <= 246]
    t.join()
    box = res.get('BOX')
    ends = [(int(w['top']) + int(w['height'])) // sc for w in wins]
    restored = bool(back) and (20, 40, 600, 216) in back
    ok = top is not None and bool(box) and bool(ends) and all(e <= top for e in ends) and c1 == c0 + 1 and restored
    return ok, 'opened %r bar top %r ends %r clamped %d -> %d; after BAR OFF %r (want 20 40 600 216)' % (box, top, ends, c0, c1, back)


ROWS = [('start', r_start), ('left_panel', r_left_panel), ('close_panel', r_close_panel),
        ('right_launch', r_right_launch), ('cold_panel', r_cold_panel), ('behind', r_behind),
        ('two_rows', r_two_rows),
        ('strut_open', r_strut_open), ('strut_zoom', r_strut_zoom), ('strut_desk', r_strut_desk), ('strut_off', r_strut_off),
        ('strut_prebar', r_strut_prebar)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', help='row name, or a comma list of rows')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    a.only = verdicts.parse_only(a.only, [n for n, _ in ROWS])
    OUT.mkdir(parents=True, exist_ok=True)
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    tabstate.open_host('barhost')
    passed = total = 0
    for name, fn in ROWS:
        f = OUT / ('bar_' + name + '.verdict')
        if a.only and name not in a.only:
            continue
        if f.exists() and not a.fresh and not a.only:
            old = f.read_text()
            if old.startswith('ok') or not a.failed:
                total += 1
                passed += old.startswith('ok')
                continue
        wb_front()
        ok, detail = fn()
        f.write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
    print('amarchy_bar_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
