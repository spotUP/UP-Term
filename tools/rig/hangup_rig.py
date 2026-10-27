#!/usr/bin/env python3
"""hangup_rig.py -- closing a vtcon window hangs up its foreground program
(todos WINDOW-CLOSE-KILLS-APP: closing the window left mg / nano running).

Opens a vsh window, runs a full-screen ixemul editor (nano, mg when installed) in it, closes the
window with its close gadget and checks that no process of that program is left. Self-checking.
Also closes a plain vsh window (nothing running) and checks it ends. The rig must be up
(`UPTERM_RIG=<n>`), handler installed (`rig.py install` after `make amiga`).

  hangup_rig.py [nano|mg|vsh ...]"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, vshwin

PROG = {'nano': 'UP-Term:bin/nano', 'mg': 'UP-Term:bin/mg'}


def procs(name):
    return [l for l in ixpty_rig.run('Status')[1].splitlines() if name in l]


def close_by_gadget(title):
    w = ami.window(title)
    if not w or 'sys:close' not in w:
        return False
    x, y, cw, ch = w['sys:close']
    ami.click_px(x + cw // 2, y + ch // 2)
    return True


def gone(title, secs=15):
    end = time.time() + secs
    while time.time() < end:
        if not ami.window(title):
            return True
        time.sleep(1)
    return False


def case(prog):
    title = 'hangup' + prog
    if prog != 'vsh' and ixpty_rig.run('List %s' % PROG[prog])[0] != 0:
        print('%s: SKIP (%s not installed)' % (title, PROG[prog]))
        return True
    vshwin.open_vsh(title, "0/20/600/200", wait=7)
    if prog == 'vsh':
        before = procs('vsh')
    else:
        vshwin.typeline(PROG[prog], 5)
        before = procs(prog.split(':')[-1])
        if not before:
            print('%s: FAIL (program did not start)' % title)
            return False
    if not close_by_gadget(title):
        print('%s: FAIL (no window or no close gadget)' % title)
        return False
    win = gone(title)
    time.sleep(5)
    name = 'vsh' if prog == 'vsh' else prog
    left = procs(name)
    ok = win and (not left if prog != 'vsh' else len(left) < len(before))
    print('%s: running before close %s; window gone %s; left after close %s: %s'
          % (title, before, win, left, 'ok' if ok else 'FAIL'))
    return ok


def main():
    progs = [a for a in sys.argv[1:] if a in ('nano', 'mg', 'vsh')] or ['nano', 'mg', 'vsh']
    ok = True
    for p in progs:
        ok = case(p) and ok
    print('hangup_rig: %s' % ('OK' if ok else 'FAIL'))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
