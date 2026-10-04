#!/usr/bin/env python3
"""mouse_rig.py -- ledger W18: mouse reports end to end on the rig.

A program asks for button reports in the SGR form (?1000 / ?1002 + ?1006,
written by rawprobe, which then prints every byte it reads as hex):
  1. ?1000+?1006: a left click at a known cell reads ESC [ < 0 ; col ; row M
     then the release ... m, with col/row the clicked cell (1-based).
  2. ?1002+?1006: a drag across cells adds motion reports (button 32+0).
  3. Without any mode: a click sends nothing to the program.
Wheel reports (buttons 64/65) need a wheel event the agent cannot send yet:
not covered.

Run with the rig up and the handler installed:
  python3 tools/rig/mouse_rig.py
"""
import os, re, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

HERE = os.path.dirname(os.path.abspath(__file__))
TITLE = 'mouse'
fails = []


def check(name, ok, detail=''):
    print('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, detail if not ok else ''), flush=True)
    if not ok:
        fails.append(name)


def run(cmd, timeout=30):
    return ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))[4:].decode('latin-1')


def get(path):
    tmp = os.path.join(HERE, '_mouserig_get')
    if os.path.exists(tmp):
        os.unlink(tmp)
    ami.main(['get', path, tmp])
    data = open(tmp, 'rb').read() if os.path.exists(tmp) else b''
    if os.path.exists(tmp):
        os.unlink(tmp)
    return data


def typeline(s, wait=1.5):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(wait)


def cell_xy(col, row):
    """screen pixel of the middle of cell (col, row), 1-based; topaz 8 cells
    are 8x8, the text area starts inside the left border and below the
    title bar (the window's sys:drag gadget gives the title height)"""
    w = ami.window(TITLE)
    x, y, _, _ = w['box']
    th = w.get('sys:drag', (0, 0, 0, 11))[3] + 1
    return x + 4 + (col - 1) * 8 + 4, y + th + (row - 1) * 8 + 4


def probe(seq, tag, events):
    out = 'RAM:mouse_%s.txt' % tag
    typeline('VTC:rawprobe * "%s" 5 >%s' % (seq, out), wait=1.5)
    kx, ky = ami.pointer_scale()
    ev = []
    for e in events:
        if e[0] == 'move':
            px, py = cell_xy(e[1], e[2])
            ev.append(('move', int(px * kx), int(py * ky)))
        else:
            ev.append(e)
    ami.script(*ev)
    time.sleep(8)
    hexes = get(out).decode('latin-1').split()
    return bytes(int(h, 16) for h in hexes if re.fullmatch('[0-9a-f]{2}', h)).decode('latin-1')


def reports(s):
    return re.findall(r'\x1b\[<(\d+);(\d+);(\d+)([Mm])', s)


def main():
    run('run >NIL: newshell "XCON:0/12/640/200/%s/CLOSE"' % TITLE)
    for _ in range(5):
        time.sleep(2)
        if ami.window(TITLE):
            break
    else:
        raise SystemExit('window never appeared')
    typeline('Clear')

    click = [('move', 10, 12), ('wait', 3), ('button', 0, 1), ('wait', 3), ('button', 0, 0), ('wait', 5)]
    got = reports(probe(r'\e[?1000h\e[?1006h', 'click', click))
    check('click: press and release reported', [r[3] for r in got] == ['M', 'm'], repr(got))
    if got:
        b, col, row, _ = got[0]
        check('click: left button, the clicked cell (10,12) +-1',
              b == '0' and abs(int(col) - 10) <= 1 and abs(int(row) - 12) <= 1, repr(got))

    drag = [('move', 5, 14), ('wait', 3), ('button', 0, 1), ('wait', 3), ('move', 9, 14), ('wait', 3),
            ('move', 15, 14), ('wait', 3), ('button', 0, 0), ('wait', 5)]
    got = reports(probe(r'\e[?1000l\e[?1002h\e[?1006h', 'drag', drag))
    motion = [r for r in got if r[0] == '32']
    check('drag: motion reports with button 32', len(motion) >= 1, repr(got))
    check('drag: ends with a release', bool(got) and got[-1][3] == 'm', repr(got))

    got = probe(r'\e[?1002l\e[?1006l', 'none', click)
    check('no mode: a click sends nothing', got == '', repr(got))

    typeline('EndShell', wait=2)
    run('Delete >NIL: RAM:mouse_#?.txt QUIET')
    print('%d failed' % len(fails))
    sys.exit(1 if fails else 0)


main()
