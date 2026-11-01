#!/usr/bin/env python3
"""s28 FLAT-BUTTON-ROUND-BG on a rig (UPTERM_RIG=<n>, 3.2, `rig.py aga256`, Amarchy running, `make build/amiga/btnbgprobe`):
flat on, then RAM:btnbgprobe draws a 2x2 checker of pens 2 and 3 (never pen 0) in a window, with and without one 80x14
GadTools button. Corner pixels of the button's box (the 4 corners and the 8 cuts of a button 12 px or more high) are read
from the screen grab and compared with the same pixels of the pattern-only grab: the pixel outside the rounded edge must
be the background pen that was under it, in the idle state and with the left mouse button held on the button.
Prints [PASS]/[FAIL] lines; verdicts and pixels also go to the stdout for the evidence file. Needs no human."""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_desk_rig as d

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
fails = 0


def verdict(ok, what):
    global fails
    fails += not ok
    print('[%s] %s' % ('PASS' if ok else 'FAIL', what), flush=True)


def probe(mode):
    """Start btnbgprobe MODE detached; returns ((wx, wy, ww, wh), (gx, gy, gw, gh)) from its WIN/GAD lines."""
    c.run('Delete >NIL: T:btnbg.out QUIET')
    c.run('Run >NIL: <NIL: RAM:btnbgprobe %s' % mode)
    for _ in range(40):
        time.sleep(0.5)
        out = (ami.read_file('T:btnbg.out') or b'').decode('latin-1')
        if 'READY' in out:
            break
    else:
        raise SystemExit('btnbgprobe %s never printed READY: %r' % (mode, out))
    win = re.search(r'WIN (\d+) (\d+) (\d+) (\d+)', out)
    gad = re.search(r'GAD (\d+) (\d+) (\d+) (\d+)', out)
    time.sleep(1.5)
    return tuple(map(int, win.groups())), tuple(map(int, gad.groups()))


def stop_probe():
    for m in re.finditer(r'Process\s+(\d+):.*btnbgprobe', c.run('Status')[1]):
        c.run('Break %s C' % m.group(1))
    time.sleep(2)


def grab():
    w, h, rows = ami.grab()
    return w, h, rows


def px(rows, x, y):
    return rows[y][3 * x:3 * x + 3].hex()


def corners(g):
    """The 12 pixels a rounded button box (x0,y0,w,h; h >= 12) cuts, each with the pixel outside the box the product reads
    for it (flat_button_corners in amarchy/flat_glyph.c: diagonal for a corner, above/left of the box for the two cuts)."""
    x0, y0, w, h = g
    out = []
    for cx in (0, 1):
        for cy in (0, 1):
            x = x0 + (w - 1 if cx else 0)
            y = y0 + (h - 1 if cy else 0)
            ix = -1 if cx else 1
            iy = -1 if cy else 1
            out.append(((x, y), (x - ix, y - iy)))
            out.append(((x + ix, y), (x + ix, y - iy)))
            out.append(((x, y + iy), (x - ix, y + iy)))
    return out


def compare(tag, base, now, g, pens):
    names = {v: 'pen%d' % k for k, v in pens.items()}
    bad = 0
    for (x, y), (sx, sy) in corners(g):
        b, n = px(base, sx, sy), px(now, x, y)
        line = '  %s (%d,%d) pen outside at (%d,%d) %s (%s) now %s (%s)' % (tag, x, y, sx, sy, b, names.get(b, '?'), n, names.get(n, '?'))
        print(line + ('' if b == n else '   <-- differs'), flush=True)
        bad += b != n
    return bad


def main():
    ami.put(os.path.join(ROOT, 'build', 'amiga', 'btnbgprobe'), 'RAM:btnbgprobe')
    ami.put(os.path.join(ROOT, 'build', 'amiga', 'wbpens'), 'T:wbpens')
    ami.put(os.path.join(ROOT, 'build', 'amiga', 'amarchy-flat'), 'RAM:amarchy-flat')
    c.run('Protect RAM:amarchy-flat +e')
    c.run('MakeDir >NIL: ENV:Amarchy')
    if not d.flat_stop():
        raise SystemExit('a resident amarchy-flat did not unload')
    d.put_flat_prefs(False)
    c.run('Run >NIL: RAM:amarchy-flat')
    time.sleep(3)
    d.put_flat_prefs(True)
    print('[INFO] theme pick -> %s' % (d.pick(),), flush=True)
    word, calls, pens, raw = d.flat_pens()
    print('[INFO] amarchy-flat STATUS: %s' % raw.replace('\n', ' | '), flush=True)
    cols = d.wb_colours()
    print('[INFO] Workbench colours 0-3 %s depth %s' % (cols, d.wb_depth()), flush=True)
    palette = {k: v for k, v in cols.items()}
    # the grab is RGB at 8 bits; wbpens prints 8-bit hex pairs (rrggbb)
    verdict(word == 'on', 'flat is on (STATUS flat=%s)' % word)
    win, g = probe('NOGAD')
    print('[INFO] window %s button box %s' % (win, g), flush=True)
    _, _, base = grab()
    stop_probe()
    win2, g2 = probe('GAD')
    verdict((win2, g2) == (win, g), 'same window and button place in the second run')
    word, calls1, _, _ = d.flat_pens()
    _, _, idle = grab()
    ctx, cty = g[0] + g[2] // 2, g[1] + g[3] // 2
    sx, sy = ami.pointer_scale()
    ami.script(('move', int(ctx * sx), int(cty * sy)), ('button', 0, 1))
    time.sleep(1.5)
    _, _, held = grab()
    ami.script(('button', 0, 0))
    word, calls2, _, _ = d.flat_pens()
    print('[INFO] frame calls before/after the button windows: %s -> %s' % (calls, calls2), flush=True)
    verdict(calls2 is not None and calls1 is not None and calls2 >= calls1 and calls2 > (calls or 0), 'flat hook ran for the button window (calls grew)')
    x0, y0, w, h = g
    mid = px(idle, x0 + w // 2, y0 + 3)
    print('[INFO] idle fill pixel (mid, row 3) %s; held fill pixel %s; pattern pens %s' % (mid, px(held, x0 + w // 2, y0 + 3), palette), flush=True)
    verdict(mid not in (px(base, x0 + w // 2, y0 + 3),), 'a button is drawn (centre differs from the pattern)')
    pa = {k: v for k, v in palette.items() if k in (0, 2, 3)}
    print('[INFO] background samples: ' + ', '.join('(%d,%d)=%s' % (x, y, px(base, x, y)) for (x, y), _ in corners(g)[:4]), flush=True)
    verdict(compare('idle', base, idle, g, palette) == 0, 'idle: 12 corner pixels carry the background pen under them')
    verdict(compare('held', base, held, g, palette) == 0, 'pressed: 12 corner pixels carry the background pen under them')
    held_vs_idle = sum(px(held, x, y) != px(idle, x, y) for y in range(y0 + 2, y0 + h - 2) for x in range(x0 + 2, x0 + w - 2))
    verdict(held_vs_idle > 0, 'the press changed the button interior (%d px differ idle vs held)' % held_vs_idle)
    stop_probe()
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
