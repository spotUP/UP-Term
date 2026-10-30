#!/usr/bin/env python3
"""pfind_rig.py -- ixemul kill() finds a pid only in the exec task lists (self-checking).

pfind() walked TaskWait and TaskReady with `for (n = head; n->ln_Succ; ...)` and then tested n for NULL: the
walk ends on the tail node, never NULL, so TaskReady was never searched and EVERY address counted as found.
kill() then read tc_TrapData of any address; `ixkill -0 <live native task>` hung for more than 40 s and
ignored Ctrl-C. Must hold, each inside 5 s:
  ixkill -0 <every other task in TaskWait/TaskReady>  -> rc 0 for the ixemul process, rc 1 (ESRCH) for the rest
  kill(getpid(), 0)                                   -> 0 (the running task is in neither exec list)
  ixkill -0 <live ixemul process>                     -> rc 0 (ixpfind wait, started with Run)
A request that gets no answer in 5 s is the hang: FAIL on the old library.

  UPTERM_RIG=4 [IXEMUL=<ixemul.library> IXNET=<ixnet.library>] python3 -I tools/rig/pfind_rig.py

The rig must be up, booted with that ixemul; `make build/amiga/ixkill build/amiga/ixpfind` first."""
import sys, os, pathlib, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"


def check(label, cmd, want_rc, want_text=None):
    try:
        rc, out = ixpty_rig.run(cmd, 5)
    except Exception as e:  # no answer: the old library hangs here
        print('%s: no answer in 5 s (%s) FAIL' % (label, e))
        return False
    good = rc == want_rc and (want_text is None or want_text in out)
    print('%s: rc %d (want %d) %s' % (label, rc, want_rc, 'ok' if good else 'FAIL ' + out.strip().replace('\n', ' | ')))
    return good


def main():
    for name in ("ixkill", "ixpfind"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    ixpty_rig.use_ixemul()
    ok = check("self: kill(getpid(), 0)", 'VTC:ixpfind self', 0, 'kill rc 0')
    ixpty_rig.run('Delete RAM:pfind.pid QUIET')
    ixpty_rig.run('Run >NIL: VTC:ixpfind wait')
    ixpty_rig.run('Wait 3')
    rc, out = ixpty_rig.run('Type RAM:pfind.pid', 5)
    peer = out.strip()
    ok = check("live ixemul process %s: ixkill -0" % peer, 'VTC:ixkill -0 %s' % peer, 0) and ok
    ixpty_rig.run('Run >NIL: Wait 120')
    ixpty_rig.run('Wait 2')
    rc, out = ixpty_rig.run('VTC:ixpfind list', 10)
    tasks = [l.split(None, 1) for l in out.splitlines() if l.strip()]
    print('%d other tasks in the exec lists' % len(tasks))
    for addr, name in tasks:
        want = 0 if addr == peer else 1
        ok = check("task %s %s: ixkill -0" % (addr, name), 'VTC:ixkill -0 %s' % addr, want) and ok
    ixpty_rig.run('VTC:ixkill -KILL %s' % peer, 5)
    print('pfind_rig: %s (ixemul %s)' % ('OK' if ok else 'FAIL', ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
