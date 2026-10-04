#!/usr/bin/env python3
"""cc_rig.py -- CC1 and CC2 reached through vtwin's flush (ledger S1, CUSTOM
CHIPS): the scroll by blitter copy with the vacated rows filled by the CPU
painter, and the cursor as a hardware sprite. The sentinel is the
renderer's own call counters, written by the handler when the window's
handles close:

  render blitscroll/owed      scrolls done by the BltBitMap copy, owed rows filled
  render spritemove/image     sprite cursor moves, sprite images made
  render planecursor/why      plane cursors drawn, why the last was not a sprite
                              (render/chips.h VC_CUR_*: 0 sprite, 1 RTG, 2 hidden,
                              3 geometry, 4 position, 5 cell, 6 image, 7 colours,
                              8 no sprite free)

Needs a handler built with `make build/amiga/vtcon-handler SERIAL=1` and
installed (rig.py install), and the stock rig (rig.py start --stock --os32).
Boots fresh, opens an UP-Term window on a PAL hires screen of its own (4
planes, the sprite's colours 21-22 are free there) and one on the
Workbench, lists a long directory in each (scrolls), takes a screenshot of
each (build/rig/shots/cc-own.png, cc-wb.png: the text must have no stale
rows, the cursor a block after the prompt), closes them and checks the
counters. PASS needs blitscroll > 0 and owed > 0 on the own screen, and
sprite images > 0 with the reason 0 in the window that was active.

  cc_rig.py            (no arguments)
"""
import pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami
import condev_rig as c
import conbench_rig as cb

ROOT = pathlib.Path(__file__).resolve().parents[2]
SER = ROOT / 'build/rig/serial.log'
OUT = ROOT / 'build/rig/shots'
WINDOWS = [
    ('cc-own', 'XCON:0/0/640/200/ccown/OWNSCREEN/SCREENMODE 0x29000/DEPTH 4/AMIGA'),
    ('cc-wb', 'XCON:0/12/640/180/ccwb/AMIGA'),
]


def counters(log):
    """Each closed window's three counter lines, in the order they came."""
    out, cur = [], {}
    for l in log:
        m = re.match(r'v render (blitscroll/owed|spritemove/image|planecursor/why) ([0-9a-f]{8}) ([0-9a-f]{8})', l.strip())
        if not m:
            continue
        cur[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
        if m.group(1) == 'planecursor/why':
            out.append(cur)
            cur = {}
    return out


def main():
    cb.rig('stop')
    SER.write_bytes(b'')  # FS-UAE rewrites the file from its start each boot
    cb.rig('start')
    OUT.mkdir(parents=True, exist_ok=True)
    for name, spec in WINDOWS:
        title = spec.split('/')[4]
        c.run('Run >NIL: NewShell "%s"' % spec)
        time.sleep(8)
        cb.click(title)  # active: the sprite cursor is only the active window's
        cb.typeline('List SYS: ALL')
        time.sleep(40)
        cb.typeline('Echo done')
        time.sleep(3)
        ami.main(['shot', str(OUT / (name + '.png'))])
        cb.typeline('EndCLI')
        time.sleep(4)
    cb.rig('stop')  # FS-UAE buffers the serial file: stopping flushes it
    log = SER.read_bytes().decode('latin-1', 'replace').splitlines()
    seen = counters(log)
    for k in seen:
        print(k)
    ok = any(k.get('blitscroll/owed', (0, 0))[0] > 0 and k['blitscroll/owed'][1] > 0 for k in seen)
    print('%s CC1: a scroll by blitter copy, its vacated rows filled by the painter' % ('ok' if ok else 'FAIL'))
    spr = any(k.get('spritemove/image', (0, 0))[1] > 0 and k.get('planecursor/why', (0, 9))[1] == 0 for k in seen)
    print('%s CC2: the cursor as a hardware sprite' % ('ok' if spr else 'FAIL'))
    print('look at build/rig/shots/cc-own.png and cc-wb.png: no stale rows, the cursor where the prompt ends')
    return 0 if ok and spr else 1


if __name__ == '__main__':
    sys.exit(main())
