#!/usr/bin/env python3
"""histfile_rig.py -- the console's history file on the rig (handler/histfile.h, owner 2026-10-10).

1. With the kit's assign (UP-Term: put on RAM:hfkit for the run), an ENVARC:vtcon.history of an older UP-Term
   moves to UP-Term:var/vtcon/history when a window opens, and a line typed at that window's AmigaShell is
   appended there.
2. A window started by vshwin (HISTFILE=RAM:rig.history) writes the lines typed to vsh into RAM:rig.history and
   nothing into the default file.
The rig's own ENVARC:vtcon.history is put back at the end, and the assign removed. The rig must be up with the
current handler installed (rig.py install) and VTC:vsh current; UPTERM_RIG=2 for rig 2."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, screen_rig, vshwin

OLD, NEW = "ENVARC:vtcon.history", "UP-Term:var/vtcon/history"


def run(cmd):
    return ixpty_rig.run(cmd, 30)


def text(path):
    rc, out = run("Type %s" % path)
    return out.replace("\r", "") if rc == 0 else None


def active(title):
    w = ami.window(title)
    return bool(w and w["active"])


def main():
    ok = True
    if run("Assign >NIL: UP-Term: EXISTS")[0] == 0:
        print("FAIL UP-Term: is assigned on this rig already: the run would move the real history")
        return 2
    had = run("Copy %s RAM:hf-save QUIET" % OLD)[0] == 0
    try:
        run("MakeDir RAM:hfkit")
        run("Assign UP-Term: RAM:hfkit")
        run("Delete %s RAM:rig.history QUIET" % OLD)
        run('Echo "old-line" >%s' % OLD)
        run('Run >NIL: NewShell "XCON:0/20/600/150/histfile default/CLOSE"')
        time.sleep(5)
        moved = text(NEW)
        good = moved is not None and "old-line" in moved and text(OLD) is None
        print("%s the old ENVARC: history moved to %s (%r)" % ("ok" if good else "FAIL", NEW, moved))
        ok &= good
        if active("histfile default"):
            screen_rig.typeline("echo typed-line", 2)
            got = text(NEW) or ""
            good = "typed-line" in got
            print("%s a line typed at the AmigaShell went to %s" % ("ok" if good else "FAIL", NEW))
            ok &= good
            screen_rig.typeline("endcli", 2)
        else:
            print("FAIL the window is not the active one: nothing typed")
            ok = False
        before = text(NEW) or ""
        vshwin.open_vsh("histfile vsh", "0/20/600/150", wait=8)
        if active("histfile vsh"):
            screen_rig.typeline("echo vsh-line", 2)
            mine = text(vshwin.HISTFILE) or ""
            after = text(NEW) or ""
            good = "echo vsh-line" in mine and after == before
            print("%s vsh's lines went to %s only (%r; default grew %d bytes)" %
                  ("ok" if good else "FAIL", vshwin.HISTFILE, mine.strip(), len(after) - len(before)))
            ok &= good
            screen_rig.typeline("exit", 2)
        else:
            print("FAIL the vsh window is not the active one: nothing typed")
            ok = False
    finally:
        run("Assign UP-Term: REMOVE")
        run("Delete RAM:hfkit ALL QUIET")
        run("Delete %s QUIET" % OLD)
        if had:
            run("Copy RAM:hf-save %s QUIET" % OLD)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
