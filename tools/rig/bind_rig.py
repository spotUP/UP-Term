#!/usr/bin/env python3
"""bind_rig.py -- vsh's `bind` and .inputrc reach the console's line editor (V92).

Opens an XCON: window with VTC:vsh. `bind '"\\C-a": end-of-line'` makes Ctrl-A go to the end of the
line (emacs's Ctrl-A goes to its start); a vsh started with INPUTRC naming a file with
`set editing-mode vi` edits in vi mode from its first line. Each case writes a file in RAM:, read
back here. The rig must be up with the current handler installed (rig.py install, then a reboot);
UPTERM_RIG=2 for rig 2.

  bind_rig.py"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, screen_rig

ESC, CTRL_A, RET = "esc", "ctrl-a", None
CTRL = 0x0008

CASES = (
    ("bound Ctrl-A", ["bind '\"\\C-a\": end-of-line'", RET, "echo a", CTRL_A, "b >RAM:b1.txt", RET],
     "RAM:b1.txt", "ab"),
    ("Ctrl-A unbound again", ["bind -r '\\C-a'", RET, "echo c", CTRL_A, "d >RAM:b2.txt", RET],
     "RAM:b2.txt", "cd"),  # bind -r: Ctrl-A does nothing, d goes at the end
    ("inputrc vi", ["INPUTRC=RAM:b.irc VTC:vsh", RET, "echo abc >RAM:b3.txt", ESC, "0wcwxyz", ESC, RET,
                    "exit", RET], "RAM:b3.txt", "xyz"),
)


def send(step):
    if step is RET:
        ami.key(0x44)
        time.sleep(2.5)
    elif step == ESC:
        ami.key(0x45)
        time.sleep(0.5)
    elif step == CTRL_A:
        ami.key(0x20, CTRL)
        time.sleep(0.5)
    else:
        ami.req(0x08, bytes([4]) + step.encode())
        time.sleep(0.6)


def main():
    ixpty_rig.use_ixemul()
    ixpty_rig.run("Delete RAM:b1.txt RAM:b2.txt RAM:b3.txt QUIET")
    ixpty_rig.run('Echo "set editing-mode vi" >RAM:b.irc')
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/600/200/bind/CLOSE"', 10)
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
