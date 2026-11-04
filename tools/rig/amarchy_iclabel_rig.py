#!/usr/bin/env python3
"""RL21 SELECTA-AA-ICONTEXT on a rig (UPTERM_RIG=<n>, `rig.py start --max`): amarchy-flat antialiases Workbench and DOpus
icon labels through its Text() patch. Opens a SYS: drawer with flat on, grabs it, quits amarchy-flat (stock Text back),
reopens the drawer and grabs again. In the drawer's inner area: pixels that differ between the two grabs, how many of
them are the stock background in the flat-off grab (the empty edge pixels AA fills), and how many hold a colour that is
in neither grab's two most common colours (intermediate shades). Also the STATUS labels= / lskip= counters, that the
Workbench task survives, and that amarchy-flat is gone after Break. Prints [PASS]/[FAIL]; shots go to --shots DIR."""
import os, sys, time, re
from collections import Counter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_desk_rig as d

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
fails = 0


def verdict(ok, what):
    global fails
    fails += not ok
    print('[%s] %s' % ('PASS' if ok else 'FAIL', what), flush=True)


def drawer(open_it):
    """Open or close the SYS: drawer through the Workbench ARexx port (3.x) and, on a DOpus desktop, DOpus's."""
    act = 'OPEN' if open_it else 'CLOSE'
    c.run('rx "ADDRESS WORKBENCH; WINDOW SYS: %s"' % act, timeout=20)
    time.sleep(3)


def find_box():
    for t in ('Workbench', 'System', 'SYS', 'Workbench3.2', 'Workbench3.1'):
        try:
            b = ami.window(t, prefix=True)
        except Exception:
            b = None
        if b:
            return t, b
    return None, None


def inner(rows, box, w):
    x, y, bw, bh = box
    out = {}
    for yy in range(y + 22, y + bh - 12):
        r = rows[yy]
        for xx in range(x + 8, x + bw - 20):
            out[(xx, yy)] = tuple(r[xx * 3:xx * 3 + 3])
    return out


def main():
    shots = sys.argv[sys.argv.index('--shots') + 1] if '--shots' in sys.argv else None
    tag = os.environ.get('UPTERM_RIG', 'x')
    ami.put(os.path.join(ROOT, 'build', 'amiga', 'amarchy-flat'), 'RAM:amarchy-flat')
    c.run('Protect RAM:amarchy-flat +e')
    c.run('MakeDir >NIL: ENV:Amarchy')
    d.flat_stop()
    d.put_flat_prefs(True)
    c.run('Run >NIL: RAM:amarchy-flat')
    time.sleep(4)
    drawer(False)
    drawer(True)
    word, calls, pens, raw = d.flat_pens()
    print('[INFO] STATUS on: %s' % raw.replace('\n', ' | '), flush=True)
    m = re.search(r'labels=(\d+) lskip=(\d+)', raw)
    labels = int(m.group(1)) if m else -1
    verdict(labels > 0, 'icon labels antialiased: labels=%d lskip=%s' % (labels, m.group(2) if m else '?'))
    title, box = find_box()
    print('[INFO] drawer window %r box %s' % (title, box), flush=True)
    w, h, on_rows = ami.grab()
    if shots:
        ami.png(os.path.join(shots, 'rl21-rig%s-aa-on.png' % tag), w, h, on_rows)
    st = c.run('Status')[1]
    verdict(re.search(r'Workbench|dopus', st, re.I) is not None, 'Workbench/DOpus task alive after the AA labels')
    gone = d.flat_stop()
    verdict(gone, 'amarchy-flat unloads on Break (stock Text back)')
    drawer(False)
    drawer(True)
    title2, box2 = find_box()
    w2, h2, off_rows = ami.grab()
    if shots:
        ami.png(os.path.join(shots, 'rl21-rig%s-aa-off.png' % tag), w2, h2, off_rows)
    if not box:
        box = (0, 12, w, h - 12)
    a, b = inner(on_rows, box, w), inner(off_rows, box, w)
    top_on = [k for k, _ in Counter(a.values()).most_common(2)]
    top_off = [k for k, _ in Counter(b.values()).most_common(2)]
    bg = top_off[0]
    diff = [p for p in a if a[p] != b.get(p)]
    from_bg = [p for p in diff if b.get(p) == bg]
    mid = [p for p in from_bg if a[p] not in top_on and a[p] not in top_off]
    off_cols = len(set(b.values()))
    on_cols = len(set(a.values()))
    print('[INFO] inner area %d px; colours on %d off %d; top on %s off %s' % (len(a), on_cols, off_cols, top_on, top_off), flush=True)
    print('[INFO] differ %d; of those stock-background %d; intermediate shades %d; samples %s' % (
        len(diff), len(from_bg), len(mid), [(p, a[p]) for p in mid[:4]]), flush=True)
    verdict(len(mid) > 0, 'AA on: %d edge pixels in an intermediate colour where stock has the background' % len(mid))
    verdict(box2 == box or box2 is None, 'drawer reopened at the same place (%s)' % (box2,))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
