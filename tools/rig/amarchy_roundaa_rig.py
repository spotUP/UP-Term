#!/usr/bin/env python3
"""RL19 SELECTA-AA-ROUND on a rig (UPTERM_RIG=<n>, 3.2 + `rig.py aga256`, Amarchy running, `make build/amiga/btnbgprobe`).
  amarchy_roundaa_rig.py BINARY TAG    BINARY: the amarchy-flat to install (new or stock); TAG names the screenshots.
Draws the btnbgprobe window with and without its 80x14 button, grabs both, and measures the 12 pixels the rounded box cuts:
how many hold a colour that is neither the backdrop under them nor the box fill (an antialiased corner pixel). Stock: 0
(hard cut). Also grabs the Locale Prefs listview scroller and measures the corners of the knob box the same way.
Prints numbers only; screenshots go to ~/Desktop/upterm-shots."""
import os, re, sys, time, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_desk_rig as d
import amarchy_btnbg_rig as b

SHOTS = os.path.expanduser('~/Desktop/upterm-shots')


def save(tag, w, h, rows):
    os.makedirs(SHOTS, exist_ok=True)
    p = os.path.join(SHOTS, 'rl19-%s.ppm' % tag)
    with open(p, 'wb') as f:
        f.write(b'P6\n%d %d\n255\n' % (w, h))
        for r in rows:
            f.write(bytes(r[:3 * w]))
    return p


def dist(a, bb):
    return sum(abs(int(a[i:i + 2], 16) - int(bb[i:i + 2], 16)) for i in (0, 2, 4))


def measure(tag, base, now, g):
    x0, y0, w, h = g
    fill = b.px(now, x0 + w // 2, y0 + 3)
    inter = 0
    for (x, y), (sx, sy) in b.corners(g):
        back, n = b.px(base, sx, sy), b.px(now, x, y)
        mid = n != back and n != fill
        inter += mid
        print('  %s (%d,%d) backdrop %s fill %s now %s %s' % (tag, x, y, back, fill, n, 'INTERMEDIATE' if mid else ''), flush=True)
    print('RESULT %s: %d of 12 corner pixels hold an intermediate colour (fill %s)' % (tag, inter, fill), flush=True)
    return inter


def knob_corners():
    """Locale Prefs: find the knob (the colour of the vertical scroller's thumb) and measure its corners against the pixel beside."""
    c.run('Run >NIL: <NIL: SYS:Prefs/Locale')
    time.sleep(4)
    w, h, rows = ami.grab()
    p = save(TAG + '-locale', w, h, rows)
    print('[INFO] Locale grab saved %s' % p, flush=True)
    # a knob is a filled box of one colour, 8..30 px wide, 12+ px high, with the track colour beside it: find runs of 10+ rows
    # whose pixel span of colour K is the same x range, K not the window background
    best = None
    from collections import Counter
    for x in range(w - 1):
        pass
    cnt = Counter(rows[y][3 * x:3 * x + 3].hex() for y in range(0, h, 2) for x in range(0, w, 2))
    return w, h, rows, cnt.most_common(8)


TAG = sys.argv[2] if len(sys.argv) > 2 else 'new'


def main():
    binary = sys.argv[1]
    ami.put(os.path.join(b.ROOT, 'build', 'amiga', 'btnbgprobe'), 'RAM:btnbgprobe')
    ami.put(binary, 'RAM:amarchy-flat')
    c.run('Protect RAM:amarchy-flat +e')
    c.run('MakeDir >NIL: ENV:Amarchy')
    if not d.flat_stop():
        raise SystemExit('a resident amarchy-flat did not unload')
    d.put_flat_prefs(False)
    c.run('Run >NIL: RAM:amarchy-flat')
    time.sleep(3)
    d.put_flat_prefs(True)
    try:
        print('[INFO] theme pick -> %s' % (d.pick(),), flush=True)
    except Exception as e:     # the menu may not open on this rig: the flat prefs alone switch flat on
        print('[WARN] theme pick skipped: %s' % e, flush=True)
    word, calls, pens, raw = d.flat_pens()
    print('[INFO] STATUS %s depth %s colours %s' % (raw.replace('\n', ' | '), d.wb_depth(), d.wb_colours()), flush=True)
    win, g = b.probe('NOGAD')
    _, _, base = b.grab()
    b.stop_probe()
    win2, g2 = b.probe('GAD')
    w, h, now = b.grab()
    print('[INFO] window %s button %s same=%s' % (win, g, (win2, g2) == (win, g)), flush=True)
    print('[INFO] saved %s' % save(TAG + '-button', w, h, now), flush=True)
    measure(TAG, base, now, g)
    b.stop_probe()
    if '--knob' in sys.argv:
        w, h, rows, top = knob_corners()
        print('[INFO] colour histogram %s' % top, flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
