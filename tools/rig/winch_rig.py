#!/usr/bin/env python3
"""winch_rig.py -- a resized XCON: window reaches an ixemul program (W47/R7).

Installs ixemul.library (IXEMUL=, default the build295 one) as ixpty_rig
does, opens an XCON: window whose shell runs tests/amiga/ixwinch, drags the
window's size gadget and reads VTC:winch.out. Passes when the program caught
SIGWINCH and TIOCGWINSZ reports the larger size. Prints which link broke
otherwise: no signal, or no new size. The rig must be up;
`make amiga build/amiga/ixwinch` first."""
import os, pathlib, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = VTC / "winch.out"
TITLE = 'winch'


def drag(frm, dx, dy):
    kx, ky = ami.pointer_scale()
    p = lambda x, y: ('move', int(x * kx), int(y * ky))
    x, y = frm
    ev = [p(x, y), ('wait', 2), ('button', 0, 1), ('wait', 3)]
    for i in range(1, 9):
        ev += [p(x + dx * i // 8, y + dy * i // 8), ('wait', 2)]
    ev += [('wait', 3), ('button', 0, 0), ('wait', 10)]
    ami.script(*ev)


def read_out():
    try:
        return OUT.read_text().strip()
    except OSError:
        return ''


def parse(s):
    f = s.split()
    if len(f) < 6:
        return None
    sz = lambda t: tuple(int(v) for v in t.split('x'))
    return sz(f[1]), int(f[3]), sz(f[5])


def main():
    shutil.copyfile(ROOT / "build/amiga/ixwinch", VTC / "ixwinch")
    (VTC / "winch.script").write_text('VTC:ixwinch 30\nEndCLI\n')
    if OUT.exists():
        OUT.unlink()
    ixpty_rig.use_ixemul()
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/400/150/%s/CLOSE" FROM VTC:winch.script' % TITLE, 10)
    end = time.time() + 30
    while time.time() < end and not read_out():
        time.sleep(1)
    start = read_out()
    w = ami.window(TITLE)
    if not start or not w:
        print('winch_rig: FAIL (program or window did not start: %r, %r)' % (start, w))
        return 1
    x, y, ww, hh = w['sys:size']
    drag((x + ww // 2, y + hh // 2), 200, 150)
    time.sleep(6)
    got = parse(read_out())
    print('window before %s after %s' % (w['box'], (ami.window(TITLE) or {}).get('box')))
    print('ixwinch: %s' % read_out())
    if not got:
        print('winch_rig: FAIL (no report)')
        return 1
    (c0, r0), n, (c1, r1) = got
    bigger = c1 > c0 and r1 > r0
    ok = n >= 1 and bigger
    why = [] if ok else (['no SIGWINCH reached the program'] if n < 1 else []) + \
        ([] if bigger else ['TIOCGWINSZ did not report the new size'])
    print('winch_rig: %s%s (ixemul %s)' % ('OK' if ok else 'FAIL', ': ' + '; '.join(why) if why else '',
                                          ixpty_rig.IXEMUL))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
