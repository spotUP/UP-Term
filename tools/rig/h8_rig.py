#!/usr/bin/env python3
"""h8_rig.py -- ledger H8.1-H8.4 (KingCON completion follow-ups) on the rig,
through kingcon_rig.py's session (a fresh XCON: window, completion =
kingcon, what Echo wrote read back).

  H8.1 Ctrl+D prints the matches in 19-character columns and cuts a name
       over 18 characters to 15 + "...": a screenshot, checked by eye.
  H8.2 Alt+Tab offers only files with the e or s bit, never a directory or a
       data file: RAM:h8bin holds mydata (no e), myexec, myscript (s, no e),
       mysub (a directory); sorted, the first two offered are myexec and
       myscript, and the third Tab never reaches mydata or mysub.
  H8.3 a C: with a second directory (Assign ADD) is searched in full: zzc +
       Alt+Tab completes to zzcmd.
  H8.4 a resident INTERNAL command is still offered: Alia + Alt+Tab = Alias.

Run with the rig up and the handler installed:
  python3 tools/rig/h8_rig.py
"""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami, condev_rig as c
import kingcon_rig as kc

HERE = os.path.dirname(os.path.abspath(__file__))
TAB, RET, ESC, D = 0x42, 0x44, 0x45, 0x22
LALT, CTRL = 0x0010, 0x0008


SHOTS = os.path.join(paths.RIG, 'shots')


def shot(name):
    os.makedirs(SHOTS, exist_ok=True)
    ami.main(['shot', os.path.join(SHOTS, name)])


def main():
    # H8.1
    c.run('MakeDir >NIL: RAM:h8')
    for n in ('aaa', 'bbb', 'AVeryLongFileName1', 'ccc', 'ddd'):
        c.run('Echo >RAM:h8/%s x' % n)
    # by eye: KingCON's list in 19-character columns (0, 19, 38, 57 at 80
    # wide), AVeryLongFileName1 shown as "AVeryLongFileNa..." (the paste
    # route cannot read the window back: a multi-line paste arrives as one
    # line at the prompt)
    kc.session(['Echo RAM:h8/', (D, CTRL), (RET,), ('look', lambda: shot('h8_1-columns.png'))], 'kingcon')
    print('look: build/rig/shots/h8_1-columns.png', flush=True)

    # H8.2
    c.run('MakeDir >NIL: RAM:h8bin RAM:h8bin/mysub')
    c.run('Copy C:Dir RAM:h8bin/myexec')
    c.run('Echo >RAM:h8bin/mydata x')
    c.run('Protect RAM:h8bin/mydata rwd')
    c.run('Echo >RAM:h8bin/myscript x')
    c.run('Protect RAM:h8bin/myscript +s -e')
    first = kc.session(['Echo >RAM:kc.out RAM:h8bin/my', (TAB, LALT), (RET,), (RET,)], 'kingcon')
    second = kc.session(['Echo >RAM:kc.out RAM:h8bin/my', (TAB, LALT), (TAB,), (RET,), (RET,)], 'kingcon')
    third = kc.session(['Echo >RAM:kc.out RAM:h8bin/my', (TAB, LALT), (TAB,), (TAB,), (RET,), (RET,)], 'kingcon')
    kc.check(first.endswith('myexec'), 'H8.2 Alt+Tab: first offered is myexec (not mydata)', first)
    kc.check(second.endswith('myscript'), 'H8.2 Alt+Tab: second is myscript (s bit)', second)
    kc.check(not third.endswith(('mydata', 'mysub', 'mysub/')), 'H8.2 never a data file or a directory', third)

    # H8.3
    c.run('MakeDir >NIL: RAM:c2')
    c.run('Copy C:Dir RAM:c2/zzcmd')
    c.run('Assign C: RAM:c2 ADD')
    # the first scan of C: reads the whole directory: seconds on a 68020,
    # so the result is waited for before Return
    out = kc.session(['Echo >RAM:kc.out zzc', (TAB, LALT), ('look', lambda: time.sleep(8)), (RET,)], 'kingcon')
    kc.check(out == 'zzcmd', 'H8.3 Alt+Tab finds a command in the second C: directory', out)
    c.run('Assign C: RAM:c2 REMOVE')

    # H8.4
    out = kc.session(['Echo >RAM:kc.out Alia', (TAB, LALT), (RET,)], 'kingcon')
    kc.check(out == 'Alias', 'H8.4 an INTERNAL resident is still offered', out)

    c.run('Delete >NIL: RAM:h8 RAM:h8bin RAM:c2 ALL QUIET')
    c.run('Delete >NIL: ENV:up-term/up-term RAM:kc.out QUIET')
    print('h8_rig: passed %d of %d' % (kc.passed, kc.total))
    return 0 if kc.passed == kc.total else 1


if __name__ == "__main__":
    sys.exit(main())
