#!/usr/bin/env python3
"""fullscreen_rig.py -- FULLSCREEN-MENU: in fullscreen, a size change from the
menus (View > 80 x 24 / 132 x 43, the same menu_run() as /size) shrank the
backdrop window and left a bare screen around it, the menu strip and title
belonging to a screen the terminal no longer filled.

Invariant: a FULLSCREEN window is the whole screen, whatever is asked.
  1. /size 80x24 in fullscreen: the window is still the screen's size, and
     the handler answers (the slash command's reply names the reason).
  2. /size 40x12 likewise (a size that fits is the case that shrank it).
  3. A plain window's /size 80x24 still resizes it (not refused).

Resumable: each case's verdict is written to build/rig/fullscreen/<case>.txt
and a case already recorded "ok" is skipped; --rerun CASE re-runs one,
--rerun all clears every verdict.
Run with the rig up and the handler installed (rig.py install + a reboot):
  python3 tools/rig/fullscreen_rig.py [--rerun CASE|all]
"""
import pathlib, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/rig/fullscreen'
RET = 0x44


def front():
    """(front screen's name, size, [(window title, w, h)])"""
    name, size, wins = None, None, []
    for l in ami.req(0x0D).decode('latin-1').splitlines():
        f = l.split()
        if l.startswith('S ') and name is None:
            name, size = l.split('"')[-2], tuple(map(int, f[2].split('x')))
        elif l.startswith('W '):
            wins.append((l.split('"')[-2],) + tuple(map(int, f[4].split('x'))))
    return name, size, wins


def t(s, wait=3):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.5)
    ami.key(RET)
    time.sleep(wait)


def fs_case(cs):
    c.run('Run >NIL: NewShell "XCON:0/20/640/300/full/FULLSCREEN"')
    time.sleep(6)
    ok = True
    seen = []
    for sz in cs:
        t('/size ' + sz)
        name, scr, wins = front()
        seen.append((sz, scr, wins))
        ok = ok and len(wins) == 1 and wins[0][1:] == scr
    t('EndCLI', 4)
    return ok, seen


def win_case():
    """the plain window is told apart by its 640x300 outer size (a CLOSE
    XCON: window shows no title in UITREE)"""
    c.run('Run >NIL: NewShell "XCON:0/20/640/300/plain/CLOSE"')
    time.sleep(5)
    before = front()[2]
    t('/size 80x24')
    after = front()[2]
    t('EndCLI', 4)
    return any(w[1:] == (640, 300) for w in before) and not any(w[1:] == (640, 300) for w in after), (before, after)


CASES = {
    'size80x24': lambda: fs_case(['80x24']),
    'size40x12': lambda: fs_case(['40x12']),
    'plain_size': win_case,
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if '--rerun' in sys.argv:
        which = sys.argv[sys.argv.index('--rerun') + 1]
        for n in CASES:
            if which in ('all', n):
                (OUT / (n + '.txt')).unlink(missing_ok=True)
    bad = 0
    for n, fn in CASES.items():
        v = OUT / (n + '.txt')
        if v.exists() and v.read_text().startswith('ok'):
            print('ok   %s (recorded)' % n)
            continue
        ok, seen = fn()
        v.write_text('%s %s\n' % ('ok' if ok else 'FAIL', seen))
        print('%s %s %s' % ('ok  ' if ok else 'FAIL', n, '' if ok else seen), flush=True)
        bad += not ok
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
