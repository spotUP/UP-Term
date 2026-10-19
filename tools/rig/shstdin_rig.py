#!/usr/bin/env python3
"""shstdin_rig.py -- a program started by `/bin/sh -c` reads the terminal (vsh at a console prompt).

bash gives a `sh -c` child the terminal as its stdin. Each case opens its own titled XCON: window,
starts VTC:vsh there with a throwaway HISTFILE (vshwin.py), types the case's command, then the line `abc`
and Ctrl-\\ (end of file on the console), or Ctrl-D (the ctrld-* cases: under vsh a program reads the window
as a Unix terminal). A program that read the terminal wrote what it
read to a file in RAM:, read back here; one that saw end of file at once wrote nothing (and `abc`
went to the prompt). Nothing is typed unless the case's window is the active one. The rig must be
up with the current handler installed; the kit's sed (upterm-ports build/kit) is copied to
VTC:shstdin, cat is VTC:userland/bin's. The pty-* cases run the same under the ports' ptyrun (a
pseudo-terminal: vsh's input is the pty's slave, its console the window), and tests/amiga/inwho
says which handler a native program's input is. build/amiga/vsh is copied to VTC:vsh and GG:bin/sh;
`make build/amiga/vsh build/amiga/inwho` first. UPTERM_RIG=2 for rig 2.

  shstdin_rig.py [case ...]"""
import os, pathlib, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, paths, screen_rig, vshwin
import install_rig as ir

# the kit's sed and the userland's cat (ixemul programs), first on vsh's PATH
SED = paths.repo("upterm-ports") / "build/kit/userland/bin/sed"
BIN = paths.RIG / "vtc/shstdin"

CTRL = 0x0008
EOF_BS, EOF_D = "^\\", "^D"   # Ctrl-\\: end of file on the console; Ctrl-D: under vsh, as a Unix terminal

# name, command line typed at vsh's prompt (output to RAM:<name>.txt), what the file must hold
CASES = (
    ("plain-sed", "sed p >RAM:plain-sed.txt", "abc\nabc"),
    ("binsh-sed", "/bin/sh -c 'sed p' >RAM:binsh-sed.txt", "abc\nabc"),
    ("vsh-sed", "VTC:vsh -c 'sed p' >RAM:vsh-sed.txt", "abc\nabc"),
    ("vsh-cat", "VTC:vsh -c cat >RAM:vsh-cat.txt", "abc"),
    ("plain-cat", "cat >RAM:plain-cat.txt", "abc"),
    ("binsh-sed-dq", "/bin/sh -c \"sed p\" >RAM:binsh-sed-dq.txt", "abc\nabc"),
    ("plain-read", "read x; echo $x >RAM:plain-read.txt", "abc"),
    # Ctrl-D (owner 2026-10-10): end of file on an empty line, the text without a newline on a line with text
    ("ctrld-sed", "sed p >RAM:ctrld-sed.txt", "abc\nabc", ("abc", EOF_D)),
    ("ctrld-cat", "cat >RAM:ctrld-cat.txt", "abc\nxy", ("abc", "~xy", EOF_D, EOF_D)),
    ("ctrld-binsh", "/bin/sh -c 'sed p' >RAM:ctrld-binsh.txt", "abc\nabc", ("abc", EOF_D)),
)


# The same under ptyrun (upterm-ports tools/ptyrun.c): the program on a pseudo-terminal, the line
# typed on its master, then ^D. name, command, how many times `abc` must be in the stream (the
# terminal's echo counts once). Run by an AmigaDOS script in a titled window, no typing.
PTY_CASES = (
    ("pty-sed", "sed p", 3),
    ("pty-binsh-sed", "/bin/sh -c 'sed p'", 3),
    ("pty-vsh-sed", "VTC:vsh -c 'sed p'", 3),
    ("pty-vsh-cat", "VTC:vsh -c cat", 2),
    ("pty-vsh-read", "VTC:vsh -c 'read x; echo got $x $x'", 3),
    # what a native program's input is (tests/amiga/inwho): the pty, as when ptyrun starts it itself
    ("pty-inwho", "VTC:shstdin/inwho", "in=PTY "),
    ("pty-vsh-inwho", "VTC:vsh -c VTC:shstdin/inwho", "in=PTY "),
    ("pty-binsh-inwho", "/bin/sh -c VTC:shstdin/inwho", "in=PTY "),
)
PTYRUN = paths.repo("upterm-ports") / "build/tools/ptyrun.amiga"
ROOT = pathlib.Path(__file__).resolve().parents[2]


def install():
    """The vsh under test as VTC:vsh and as GG:bin/sh (ixemul's /bin/sh, rig.py install), the probe."""
    BIN.mkdir(exist_ok=True)
    shutil.copyfile(SED, BIN / "sed")
    shutil.copyfile(ROOT / "build/amiga/inwho", BIN / "inwho")
    ir.put_vsh()


def pty_cases(names):
    shutil.copyfile(PTYRUN, BIN / "ptyrun")
    (BIN / "keys").write_text("800 abc\\r\n800 \\x04\n")
    todo = [c for c in PTY_CASES if not names or c[0] in names]
    lines = ["Path VTC:shstdin VTC:userland/bin ADD"]
    for name, cmd, _ in todo:
        (BIN / (name + ".stream")).unlink(missing_ok=True)
        lines.append("ptyrun -t 20 VTC:shstdin/keys VTC:shstdin/%s.stream %s >VTC:shstdin/%s.txt" %
                     (name, cmd.replace("'", '"'), name))
    lines += ["Echo done >VTC:shstdin/pty.done", "EndCLI"]
    (BIN / "pty.done").unlink(missing_ok=True)
    (BIN / "pty.script").write_text("\n".join(lines) + "\n")
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/640/200/shstdin ptyrun/CLOSE" FROM VTC:shstdin/pty.script', 10)
    for _ in range(60 * len(todo)):
        if (BIN / "pty.done").exists():
            break
        time.sleep(1)
    ok = True
    for name, cmd, want in todo:
        f = BIN / (name + ".stream")
        got = f.read_bytes().decode("latin-1") if f.exists() else ""
        n = got.count("abc") if isinstance(want, int) else got.count(want)
        status = (BIN / (name + ".txt")).read_text(encoding="latin-1").strip() if (BIN / (name + ".txt")).exists() else ""
        good = n == want if isinstance(want, int) else n > 0
        print("%s: %r (%s) %s" % (name, got.replace("\r", "")[:60], status, "ok" if good else "FAIL (want %r)" % want))
        ok = ok and good
    return ok


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
    """End the reader (Ctrl-\\), then vsh: the window closes with it (vshwin). Each key only into that window."""
    if focus(title):
        ami.key(0x0D, CTRL)
        time.sleep(2)
    if focus(title):
        screen_rig.typeline("exit", 2)


def one(name, cmd, want, keys=("abc", EOF_BS)):
    title = "shstdin %s" % name
    ixpty_rig.run("Delete RAM:%s.txt RAM:%s.rc QUIET" % (name, name))
    vshwin.open_vsh(title, "0/20/640/200", wait=8)
    if not focus(title):
        print("%s: FAIL (its window is not the active one; nothing typed)" % name)
        return False
    screen_rig.typeline("export PATH=/VTC/shstdin:/VTC/userland/bin:$PATH", 1)
    screen_rig.typeline("%s; echo rc=$? >RAM:%s.rc" % (cmd, name), 4)
    for k in keys:
        if k in (EOF_BS, EOF_D):
            ami.key(0x0D if k == EOF_BS else 0x22, CTRL)
            time.sleep(2)
        elif k.startswith("~"):
            ami.req(0x08, bytes([4]) + k[1:].encode())  # typed without Return
            time.sleep(1)
        else:
            screen_rig.typeline(k, 3)
    time.sleep(2)
    rc, done = ixpty_rig.run("Type RAM:%s.rc" % name)
    if done.startswith("rc="):
        if focus(title):
            screen_rig.typeline("exit", 2)  # vsh, then the window closes (vshwin)
    else:
        close(title)  # the reader did not end: end it, then vsh
    rc, out = ixpty_rig.run("Type RAM:%s.txt" % name)
    got = out.replace("\r", "").strip("\n")
    good = got == want and done.startswith("rc=0")
    print("%s: %r %s %s" % (name, got, done.strip(), "ok" if good else "FAIL (want %r)" % want))
    return good


def main():
    names = sys.argv[1:]
    install()
    ok = True
    for case in CASES:
        if not names or case[0] in names:
            ok = one(*case) and ok
    if not names or any(n.startswith("pty-") for n in names):
        ok = pty_cases([n for n in names if n.startswith("pty-")]) and ok
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
