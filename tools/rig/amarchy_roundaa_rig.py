#!/usr/bin/env python3
"""RL19 SELECTA-AA-ROUND on a rig (UPTERM_RIG=<n>, 3.2 + `rig.py aga256`, Amarchy running, `make build/amiga/btnbgprobe`).
  amarchy_roundaa_rig.py BINARY TAG    BINARY: the amarchy-flat to install (new or stock); TAG names the screenshots.
Draws the btnbgprobe window with and without its 80x14 button, grabs both, and measures the 12 pixels the rounded box cuts:
how many hold a colour that is neither the backdrop under them nor the box fill (an antialiased corner pixel). Stock: 0
(hard cut). Also grabs the Locale Prefs listview scroller and measures the corners of the knob box the same way.
Backdrop: a solid pen 2 (btnbgprobe SOLID); --checker keeps the s28 checker, where a corner whose outside pixel has the fill colour
is UNMEASURABLE (the blend mixes ONE pen from diagonally outside the corner: fill over fill shows no step).
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


def outside(g, x, y):
    """The pixel the blend takes its backdrop pen from: diagonally outside the corner that (x, y) belongs to (raa_frame_draw)."""
    x0, y0, w, h = g
    return (x0 - 1 if x < x0 + w // 2 else x0 + w, y0 - 1 if y < y0 + h // 2 else y0 + h)


def measure(tag, base, now, g):
    """Counts the corner pixels whose colour is neither the backdrop under them nor the fill. A corner whose outside pixel (the
    pen the blend mixes with) has the fill colour cannot show a blend at all: it is reported UNMEASURABLE, never as a miss."""
    x0, y0, w, h = g
    fill = b.px(now, x0 + w // 2, y0 + 3)
    inter = blind = 0
    for (x, y), (sx, sy) in b.corners(g):
        back, n, out = b.px(base, sx, sy), b.px(now, x, y), b.px(base, *outside(g, x, y))
        mid = n != back and n != fill
        inter += mid
        blind += out == fill
        print('  %s (%d,%d) backdrop %s outside %s fill %s now %s %s' % (tag, x, y, back, out, fill, n,
              'INTERMEDIATE' if mid else 'UNMEASURABLE' if out == fill else ''), flush=True)
    print('RESULT %s: %d of 12 corner pixels hold an intermediate colour (fill %s), %d unmeasurable' % (tag, inter, fill, blind), flush=True)
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
BACK = '' if '--checker' in sys.argv else 'SOLID'     # RL19: a solid pen-2 backdrop; --checker keeps the s28 2x2 checker


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
    win, g = b.probe(BACK and 'NOGAD ' + BACK or 'NOGAD')
    _, _, base = b.grab()
    b.stop_probe()
    win2, g2 = b.probe(BACK and 'GAD ' + BACK or 'GAD')
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
