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
  4. FULLSCREEN-SIZE-FONT: /size COLSxROWS in fullscreen picks a font size
     whose grid is the request or the nearest one above it (never smaller),
     the window staying the screen: TIOCGWINSZ (ixwinch) in the window gives
     cols >= COLS and rows >= ROWS, and a grid that fits no font size (400x200)
     is refused with the grid unchanged.

Resumable: each case's verdict is written to build/rig/fullscreen/<case>.txt
and a case already recorded "ok" is skipped; --rerun CASE re-runs one,
--rerun all clears every verdict.
Run with the rig up and the handler installed (rig.py install + a reboot):
  python3 tools/rig/fullscreen_rig.py [--rerun CASE|all]
"""
import pathlib, re, shutil, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c, ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = paths.RIG / 'fullscreen'
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


def truth():
    """TIOCGWINSZ of the shell's window now (ixwinch, an ixemul program), (cols, rows)"""
    c.run('Delete RAM:fs-truth QUIET')
    t('VTC:ixwinch 1 >RAM:fs-truth', 4)
    m = re.search(r'now (\d+)x(\d+)', c.run('Type RAM:fs-truth')[1])
    return (int(m.group(1)), int(m.group(2))) if m else None


def font_case():
    """requests that fit: cols/rows reported >= requested and the window is the screen;
    one that cannot fit: refused, the grid as it was"""
    shutil.copyfile(ROOT / 'build/amiga/ixwinch', c.VTC / 'ixwinch')
    ixpty_rig.use_ixemul()
    c.run('Run >NIL: NewShell "XCON:0/20/640/300/fullfont/FULLSCREEN"')
    time.sleep(6)
    ok, seen = True, []
    for cols, rows in ((80, 24), (100, 30), (132, 43), (40, 12)):
        t('/size %dx%d' % (cols, rows))
        name, scr, wins = front()
        got = truth()
        good = (len(wins) == 1 and wins[0][1:] == scr and got is not None
                and got[0] >= cols and got[1] >= rows and got[0] < 2 * cols + 1 and got[1] < 2 * rows + 1)
        seen.append(((cols, rows), scr, got, good))
        ok = ok and good
    before = truth()
    t('/size 400x200')
    name, scr, wins = front()
    after = truth()
    good = len(wins) == 1 and wins[0][1:] == scr and before == after
    seen.append(('400x200 refused', before, after, good))
    ok = ok and good
    t('EndCLI', 4)
    return ok, seen


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
    'font_size': font_case,
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
