#!/usr/bin/env python3
"""amarchy_warmsrv_rig.py -- the warm menu server picks up a replaced program (menusrv_core.c ms_binary_changed).

Entry point: the Amarchy commodity's hotkey (Left Amiga + Space), typed as a user would; the drawn menu's warm
server (amarchy-gui serve) is a process of its own. Proves, on the rig:
  * a first open starts the warm server; a second open with the file unchanged keeps the SAME server process
  * after the program file is replaced (ami.put gives it a new DateStamp, as upkg and `rig.py install` do), the next
    press opens the menu (cold, from the new file), the old server process is gone and a NEW server process runs
  * the open after that goes to the new server (ENV:AMARCHY_DRAWS rises) and the server process stays the same
Self-checking. Rig up (UPTERM_RIG=<n> rig.py start --max), `make amarchy-gui amarchy-menu amarchy-index build/amiga/Amarchy`.
Run: UPTERM_RIG=<n> python3 tools/rig/amarchy_warmsrv_rig.py"""
import os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_gui_rig as g

def servers():
    """Task addresses of every running amarchy-gui (the warm server; the open menu is the same program, so call
    it with the menu closed). Status numbers are reused by the next process, a task address is not (wasabi ps)."""
    out = subprocess.run([sys.executable, os.path.join(g.HERE, 'rig.py'), 'wasabi', 'ps', 'amarchy-gui*'],
                         capture_output=True, text=True).stdout
    return sorted({l.split()[0] for l in out.splitlines() if l.startswith('0x') and 'amarchy-gui' in l})

def settle(want_one=True, secs=20):
    for _ in range(secs):
        s = servers()
        if len(s) == 1:
            return s
        time.sleep(1)
    return servers()

def open_close():
    ok = bool(g.open_menu()[1])
    g.close_menu()
    time.sleep(2)
    return ok

def main():
    g.setup()
    if g.bad:
        return 1
    g.check(open_close(), 'first open shows the menu window')
    s1 = settle()
    g.check(len(s1) == 1, 'warm server running after the first open: %s' % s1)
    open_close()
    s2 = settle()
    g.check(s2 == s1, 'unchanged file: the same server process %s -> %s' % (s1, s2))
    time.sleep(12)                              # past MS_RESTART_SECS: no throttle in the way
    ami.put(os.path.join(g.REPO, 'build/amiga/amarchy-gui'), '%s/amarchy-gui' % g.SETUP_BIN)
    open_close()
    time.sleep(4)
    s3 = settle()
    g.check(len(s3) == 1 and s3 != s1, 'program replaced: a new server process %s -> %s' % (s1, s3))
    c.run('Delete >NIL: ENV:AMARCHY_DRAWS')
    open_close()
    s4 = settle()
    g.check(g.draws() > 0, 'next open ran the new server: ENV:AMARCHY_DRAWS = %d' % g.draws())
    g.check(s4 == s3, 'and kept it: %s -> %s' % (s3, s4))
    print('FAIL' if g.bad else 'PASS')
    return 1 if g.bad else 0

if __name__ == '__main__':
    sys.exit(main())
