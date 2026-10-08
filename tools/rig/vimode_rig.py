#!/usr/bin/env python3
"""vimode_rig.py -- vsh's `set -o vi` reaches the console's line editor (V91).

Opens an XCON: window with VTC:vsh, types `set -o vi`, edits command lines with
vi keys (Esc, 0 w cw . x p r) and Return, and reads what the command wrote to
RAM:. The edit mode travels with the history-config packet (a fourth line), so a
line typed after `set -o vi` is edited in vi mode, and `set -o emacs` takes
it back (Esc then does nothing). The rig must be up with the current handler
installed (rig.py install, then a reboot); run with UPTERM_RIG=2 for rig 2.

  vimode_rig.py"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, screen_rig

ESC = 0x45

# (typed text and keys: str is typed, ESC is the Esc key; the file; what it must hold)
CASES = (
    ("cw and .", ["set -o vi", None, "echo one two >RAM:vi1.txt", ESC, "0wcwONE", ESC, "w.", None],
     "RAM:vi1.txt", "ONE ONE"),
    ("x p r", ["echo abc >RAM:vi2.txt", ESC, "0wxp0wrZ", None], "RAM:vi2.txt", "Zac"),
    ("k history", [ESC, "k", ESC, "0wcwecho2", ESC, None], "RAM:vi2.txt", "echo2"),
    ("emacs again", ["set -o emacs", None, "echo a", ESC, "b >RAM:vi3.txt", None], "RAM:vi3.txt", "ab"),
)


def send(step):
    if step is None:
        ami.key(0x44)
        time.sleep(3)
    elif step == ESC:
        ami.key(ESC)
        time.sleep(0.5)
    else:
        ami.req(0x08, bytes([4]) + step.encode())
        time.sleep(0.6)


def main():
    ixpty_rig.use_ixemul()
    ixpty_rig.run("Delete RAM:vi1.txt RAM:vi2.txt RAM:vi3.txt QUIET")
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/600/200/vimode/CLOSE"', 10)
    time.sleep(4)
    screen_rig.typeline("VTC:vsh", 4)
    ok = True
    for name, steps, path, want in CASES:
        for s in steps:
            send(s)
        rc, out = ixpty_rig.run("Type %s" % path)
        good = out.strip() == want
        print("%s: %r %s" % (name, out.strip(), "ok" if good else "FAIL (want %r)" % want))
        ok = ok and good
    screen_rig.typeline("exit", 1)
    screen_rig.typeline("endcli", 1)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
