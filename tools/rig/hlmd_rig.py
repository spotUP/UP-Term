#!/usr/bin/env python3
"""hlmd_rig.py -- hl and mdv on the rig (highlight-markdown plan, R1, the six manual checks; self-checking).

  1 listing   `hl --list` names 22 languages
  2 startup   `hl -n S:Startup-Sequence` in a vsh window: line numbers in the text, 4 or more colours in the pixels
  3 timing    `hl --color=always -n RAM:big.c >NIL:` on a 100 KB C file, seconds on THIS rig (68060 with --max; the
              plan's number is for a stock A1200: run `rig.py start --a1200` for that one)
  4 mdv       `mdv RAM:README.md` (a short fixture, so the window shows all of it) in a vsh window: a heading, link addresses, the table lines all one length and
              after dragging the size gadget the table is re-fitted (wider window,
              wider table)
  5 pager     `hlp` / `mdp` need a less on the path (not in the kit): reported as SKIP when there is none
  6 romcon    a ROM CON: window (NewShell CON:): `hl -p x.c` shows 4 or more colours; `mdv` output drawn with
              + - | only (no byte above 0x7F in the table lines, TERM unset)

Usage: UPTERM_RIG=<n> python3 -I tools/rig/hlmd_rig.py [case ...]
Rig up (RTG screen, `rig.py rtg` if the Workbench is PAL), `rig.py install` done, `make amiga build/amiga/snipprobe`
(VTC:snipprobe), `make build/amiga/vsh`. hl and mdv are copied from build/amiga/ into VTC: by this script."""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, paths, typewin, vshwin, winch_rig
import condev_rig as c

ROOT = paths.VTCON
BIG_C = 100 * 1024
FIXTURE = '''# Heading One

A paragraph with a [link](https://example.org/page) and `code`, long enough that it has to wrap at the width of a window with fewer than eighty columns.

| Name | Meaning | Notes |
|------|---------|-------|
| alpha | the first letter of the Greek alphabet | short |
| beta | the second letter, with a longer cell to make the table wide | also short |

end
'''
failed = 0
skipped = 0


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok  ' if ok else 'FAIL', what, '' if ok else ': ' + seen.strip()[-300:]), flush=True)
    return ok


def prepare():
    for name in ('hl', 'mdv'):
        ami.put(str(ROOT / 'build/amiga' / name), 'VTC:' + name)
    c.run('Protect VTC:hl RWED')
    c.run('Protect VTC:mdv RWED')
    (paths.RIG / 'hlmd-doc.md').write_text(FIXTURE)
    ami.put(str(paths.RIG / 'hlmd-doc.md'), 'RAM:README.md')
    src = (ROOT / 'engine/vtengine.c').read_bytes()
    big = bytearray()
    while len(big) < BIG_C:
        big += src
    (paths.RIG / 'hlmd-big.c').write_bytes(bytes(big[:BIG_C]))
    ami.put(str(paths.RIG / 'hlmd-big.c'), 'RAM:big.c')
    ami.put(str(paths.RIG / 'hlmd-big.c'), 'RAM:x.c')


def colours(title, need=4):
    """Distinct colours (each on 20 or more pixels) in the window's pixels: coloured output, not plain text."""
    w = ami.window(title)
    x, y, ww, hh = w['box']
    _, _, rows = ami.grab((x + 4, y + 14, max(ww - 8, 8), max(hh - 18, 8)))
    n = {}
    for r in rows:
        for i in range(0, len(r) - 2, 3):
            p = bytes(r[i:i + 3])
            n[p] = n.get(p, 0) + 1
    return sorted(p.hex() for p, k in n.items() if k >= 20)


def vsh_window(title, box):
    vshwin.open_vsh(title, box, wait=7)
    typewin._last_title = title
    typewin._last_clip = typewin._clip('RAM:clip-pre.txt')
    typewin.click_in(title)
    time.sleep(1)


def line(s, wait=4):
    typewin.typ(s)
    typewin.key(typewin.RET, wait=wait)


def close_vsh(title):
    line('exit', 3)
    typewin._last_title = None


def case_listing():
    rc, out = c.run('VTC:hl --list')
    names = [l for l in out.replace('\r', '').splitlines() if l.strip()]
    check(rc == 0 and len(names) == 22, '1 listing: hl --list names 22 languages', '%d lines rc %d: %s' % (len(names), rc, out[:200]))


def case_startup():
    t = 'hlmd-startup'
    vsh_window(t, '0/20/780/500')
    line('VTC:hl -n --color=always S:Startup-Sequence', 5)
    text = typewin.screen_text()
    col = colours(t)
    check(re.search(r'^\s*1\s', text, re.M) and re.search(r'^\s*2\s', text, re.M), '2 startup: line numbers 1 and 2 in the text', text[-300:])
    check(len(col) >= 4, '2 startup: 4 or more colours in the pixels', ' '.join(col))
    close_vsh(t)


def case_timing():
    t0 = time.time()
    rc, out = c.run('VTC:hl --color=always -n RAM:big.c >NIL:', 600)
    s = time.time() - t0
    print('     100 KB C file: %.1f s (rc %d; wall clock from the Mac, includes one round trip)' % (s, rc))
    check(rc == 0, '3 timing: hl on 100 KB C ends with rc 0', 'rc %d %s' % (rc, out))


def table_of(text):
    # the clipboard text is Latin-1: the box-drawing cells come back as '?'
    rows = [l.rstrip() for l in text.replace('\r', '').splitlines()]
    return [l for l in rows if l.startswith(('|', '+', '?'))]


def case_mdv():
    t = 'hlmd-mdv'
    vsh_window(t, '0/20/520/500')
    line('VTC:mdv RAM:README.md', 6)
    a = typewin.screen_text()
    (paths.RIG / 'hlmd-mdv-a.txt').write_text(a)
    ta = table_of(a)
    wa = max([len(l) for l in ta] or [0])
    check(len(ta) >= 3 and len({len(l) for l in ta}) == 1, '4 mdv: the table is whole, every line one length', '%d lines, lengths %s' % (len(ta), sorted({len(l) for l in ta})))
    check(re.search(r'https?://', a) is not None, '4 mdv: link addresses printed', a[-200:])
    check('Heading One' in a and not re.search(r'^#+ ', a, re.M), '4 mdv: heading drawn, no # marker left', a[:200])
    winch_rig.drag((ami.window(t)['sys:size'][0] + ami.window(t)['sys:size'][2] // 2, ami.window(t)['sys:size'][1] + ami.window(t)['sys:size'][3] // 2), 240, 0)
    time.sleep(3)
    line('clear', 2)
    line('VTC:mdv RAM:README.md', 6)
    b = typewin.screen_text()
    tb = table_of(b)
    wb = max([len(l) for l in tb] or [0])
    check(len(tb) >= 3 and wb > wa, '4 mdv: after the window grew the table follows the width (wider)', 'before %d after %d' % (wa, wb))
    close_vsh(t)


def case_pager():
    rc, out = c.run('Which less')
    if rc != 0 and c.run('Exists GG:bin/less')[0] != 0:
        global skipped
        skipped += 1
        print('SKIP 5 pager: no less on the rig path (the plan: less is not in the kit)')
        return
    check(False, '5 pager: a less exists, drive hlp and mdp by hand', out)


def case_romcon():
    t = 'hlmd-romcon'
    c.run('Run >NIL: NewShell "CON:0/20/780/300/%s/CLOSE"' % t)
    time.sleep(5)
    if not ami.window(t):
        return check(False, '6 romcon: window opens')
    typewin._last_title = None
    typewin.click_in(t)
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'VTC:hl -p RAM:x.c')
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(8)
    col = colours(t)
    check(len(col) >= 4, '6 romcon: hl -p shows 4 or more colours', ' '.join(col))
    ami.req(0x08, bytes([4]) + b'EndCLI')
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(2)
    c.run('Delete RAM:md.txt QUIET')
    c.run('Run >NIL: VTC:mdv -w 70 RAM:README.md >RAM:md.txt')
    time.sleep(6)
    rc, out = c.run('Type RAM:md.txt')
    tl = [l for l in out.replace('\r', '').splitlines() if l.lstrip().startswith(('|', '+'))]
    check(len(tl) >= 3 and all(max(l.encode('latin-1')) < 0x80 for l in tl),
          '6 romcon: mdv table drawn with + - | only', '%d table lines, first %r' % (len(tl), tl[:1]))


CASES = [('listing', case_listing), ('startup', case_startup), ('timing', case_timing), ('mdv', case_mdv),
         ('pager', case_pager), ('romcon', case_romcon)]

if __name__ == '__main__':
    want = sys.argv[1:] or [n for n, _ in CASES]
    prepare()
    for name, fn in CASES:
        if name in want:
            fn()
    print('hlmd_rig: %s%s' % ('FAIL' if failed else 'PASS', ' (%d skipped)' % skipped if skipped else ''))
    sys.exit(1 if failed else 0)
