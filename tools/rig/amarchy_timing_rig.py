#!/usr/bin/env python3
"""amarchy_timing_rig.py -- Amarchy quick-menu open latency, cold and warm (plan 2026-10-22-amarchy-commodity.md,
Phase D1: the menu needs a window in under 0.3 s).

No screen reads: with `SetEnv AMARCHY_TIMING 1` the commodity and amarchy-menu append marks to RAM:amarchy.timing
(amarchy/atiming.h, off by default; the clock is DateStamp, 20 ms ticks). Per press the marks are
hotkey (the commodity got Left Amiga + Space), start (the helper's main), terminal (its XCON: window is up), ready
(the dialog is prepared, just before the first draw). open latency = ready - hotkey. The file is polled with a
cheap `Type` over wasabi.
  cold = `Avail FLUSH` before the press (libraries and fonts gone from memory)
  warm = the press after a previous one, nothing flushed
Needs the rig up (rig.py start --max). Medians go to <RIG>/amarchy/menu-timing.txt.
Usage: UPTERM_RIG=<n> python3 tools/rig/amarchy_timing_rig.py [--runs 3] [--no-build]"""
import argparse, pathlib, statistics, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig, tabstate

LAMIGA, SPACE, ESC = 0x40, 0x40, 0x45
FILE = 'RAM:amarchy.timing'
OUT = pathlib.Path(paths.RIG) / 'amarchy' / 'menu-timing.txt'


def marks():
    rc, out = c.run('Type %s' % FILE)
    res = []
    for l in out.splitlines():
        p = l.split()
        if len(p) == 2 and p[1].isdigit():
            res.append((p[0], int(p[1])))
    return res


def press(flush):
    c.run('Delete >NIL: %s QUIET' % FILE)
    if flush:
        c.run('Avail >NIL: FLUSH')
        time.sleep(1)
    ami.key(SPACE, LAMIGA)
    t0 = time.time()
    got = []
    while time.time() - t0 < 15:
        got = marks()
        if any(t == 'ready' for t, _ in got):
            break
        time.sleep(0.1)
    ami.key(ESC)            # at the root Esc leaves the menu
    time.sleep(2.5)
    d = dict(got)
    if 'hotkey' not in d or 'ready' not in d:
        return None, got
    return {t: (v - d['hotkey']) * 20 for t, v in d.items()}, got   # ticks are 1/50 s = 20 ms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runs', type=int, default=3)
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    tabstate.open_host('timinghost')
    c.run('SetEnv AMARCHY_TIMING 1')
    time.sleep(1)
    press(False)            # not counted: the first press of a fresh process pair
    lines, res = [], {}
    for kind, flush in (('cold', True), ('warm', False)):
        vals = []
        for i in range(a.runs):
            r, got = press(flush)
            print('%s %d: %s' % (kind, i + 1, r or ('NO MARKS ' + str(got))), flush=True)
            if r:
                vals.append(r)
        res[kind] = vals
        for tag in ('start', 'terminal', 'ready'):
            xs = [v[tag] for v in vals if tag in v]
            if xs:
                lines.append('%s %s median %d ms (runs %s)' % (kind, tag, statistics.median(xs), xs))
    c.run('SetEnv AMARCHY_TIMING 0')
    c.run('Delete >NIL: ENV:AMARCHY_TIMING QUIET')
    c.run('Delete >NIL: %s QUIET' % FILE)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    return 0 if all(len(v) == a.runs for v in res.values()) else 1


if __name__ == '__main__':
    sys.exit(main())
