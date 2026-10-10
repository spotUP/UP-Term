#!/usr/bin/env python3
"""shstdin_rig.py -- a program started by `/bin/sh -c` reads the terminal (vsh at a console prompt).

bash gives a `sh -c` child the terminal as its stdin. Each case opens its own titled XCON: window,
starts VTC:vsh there with a throwaway HISTFILE, types the case's command, then the line `abc` and
Ctrl-\\ (end of file on the console; Ctrl-D is not). A program that read the terminal wrote what it
read to a file in RAM:, read back here; one that saw end of file at once wrote nothing (and `abc`
went to the prompt). Nothing is typed unless the case's window is the active one. The rig must be
up with the current handler installed; the kit's sed (upterm-ports build/kit) is copied to
VTC:shstdin, cat is VTC:userland/bin's. UPTERM_RIG=2 for rig 2.

  shstdin_rig.py [case ...]"""
import os, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, paths, screen_rig

# the kit's sed and the userland's cat (ixemul programs), first on vsh's PATH
SED = paths.repo("upterm-ports") / "build/kit/userland/bin/sed"
BIN = paths.RIG / "vtc/shstdin"

CTRL = 0x0008
HIST = "RAM:shstdin.hist"

# name, command line typed at vsh's prompt (output to RAM:<name>.txt), what the file must hold
CASES = (
    ("plain-sed", "sed p >RAM:plain-sed.txt", "abc\nabc"),
    ("binsh-sed", "/bin/sh -c 'sed p' >RAM:binsh-sed.txt", "abc\nabc"),
    ("vsh-sed", "VTC:vsh -c 'sed p' >RAM:vsh-sed.txt", "abc\nabc"),
    ("vsh-cat", "VTC:vsh -c cat >RAM:vsh-cat.txt", "abc"),
    ("plain-cat", "cat >RAM:plain-cat.txt", "abc"),
    ("binsh-sed-dq", "/bin/sh -c \"sed p\" >RAM:binsh-sed-dq.txt", "abc\nabc"),
    ("plain-read", "read x; echo $x >RAM:plain-read.txt", "abc"),
)


def focus(title):
    """Make the window titled `title` the active one (a click on its drag bar); False when it is
    not there or does not become active: nothing is typed then, it would go to another window."""
    w = ami.window(title)
    if not w:
        return False
    if not w['active']:
        x, y, ww, hh = w.get('sys:drag', w['box'])
        ami.click_px(x + ww // 2, y + hh // 2)
        time.sleep(1)
        w = ami.window(title)
    return bool(w and w['active'])


def close(title):
    """End the reader (Ctrl-\\), vsh and the window's Shell."""
    if focus(title):
        ami.key(0x0D, CTRL)
        time.sleep(2)
        screen_rig.typeline("exit", 1)
        screen_rig.typeline("endcli", 2)


def one(name, cmd, want):
    title = "shstdin %s" % name
    ixpty_rig.run("Delete RAM:%s.txt RAM:%s.rc QUIET" % (name, name))
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/640/200/%s/CLOSE"' % title, 10)
    time.sleep(4)
    if not focus(title):
        print("%s: FAIL (its window is not the active one; nothing typed)" % name)
        return False
    screen_rig.typeline("Set HISTFILE %s" % HIST, 1)
    screen_rig.typeline("VTC:vsh", 4)
    screen_rig.typeline("export PATH=/VTC/shstdin:/VTC/userland/bin:$PATH", 1)
    screen_rig.typeline("%s; echo rc=$? >RAM:%s.rc" % (cmd, name), 4)
    screen_rig.typeline("abc", 3)
    ami.key(0x0D, CTRL)  # Ctrl-\: end of file on the console (Ctrl-D is not)
    time.sleep(3)
    rc, done = ixpty_rig.run("Type RAM:%s.rc" % name)
    if done.startswith("rc="):
        screen_rig.typeline("exit", 1)
        screen_rig.typeline("endcli", 2)
    else:
        close(title)  # the reader did not end: end it, then the rest
    rc, out = ixpty_rig.run("Type RAM:%s.txt" % name)
    got = out.replace("\r", "").strip()
    good = got == want and done.startswith("rc=0")
    print("%s: %r %s %s" % (name, got, done.strip(), "ok" if good else "FAIL (want %r)" % want))
    return good


def main():
    names = sys.argv[1:]
    BIN.mkdir(exist_ok=True)
    shutil.copyfile(SED, BIN / "sed")
    ok = True
    for name, cmd, want in CASES:
        if not names or name in names:
            ok = one(name, cmd, want) and ok
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
