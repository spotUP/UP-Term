#!/usr/bin/env python3
"""slashenv_rig.py -- W46d: a program started from vsh after /term or /colors sees the environment the
slash command set (handler/slash.c slash_env_list, served to vsh by vtcon_handler.c).

Each case opens its own vsh window (vshwin), types the slash command, then runs the ixemul env
(VTC:userland/bin/env) and vsh's own echo, both to RAM:, and reads them back.
  term256   /term xterm-256color   TERM=xterm-256color
  rgb       /colors rgb            COLORTERM=truecolor
  c16       /colors 16             TERM=xterm-16color, COLORTERM empty
  c16amiga  AMIGA-personality window (XCON:.../AMIGA), /colors 16: TERM=vtcon-amiga-16color, COLORTERM empty
and that the kit's xterm-16color entry is in VTC:terminfo (the rig's TERMINFO).
Rig up (UPTERM_RIG=2), handler installed and rebooted; `make amiga build/amiga/vsh`, `rig.py install`.

  UPTERM_RIG=2 python3 tools/rig/slashenv_rig.py"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, screen_rig, vshwin

CASES = (
    ("term256", ("/term xterm-256color",), "TERM=xterm-256color", None),
    ("rgb", ("/colors rgb",), None, "truecolor"),
    ("c16", ("/colors 16",), "TERM=xterm-16color", ""),
    ("c16amiga", ("/colors 16",), "TERM=vtcon-amiga-16color", ""),
)


def run(cmd):
    return ixpty_rig.run(cmd, 30)


def text(path):
    rc, out = run("Type %s" % path)
    return out.replace("\r", "") if rc == 0 else ""


def main():
    ok = True
    for name, cmds, want_term, want_ct in CASES:
        title = "slashenv " + name
        run("Delete RAM:se-%s.env RAM:se-%s.vsh QUIET" % (name, name))
        vshwin.open_vsh(title, "0/20/600/200", options="/AMIGA" if name == "c16amiga" else "", wait=8)
        w = ami.window(title)
        if not (w and w["active"]):
            print("FAIL %s: window not active" % name)
            ok = False
            continue
        for c in cmds:
            screen_rig.typeline(c, 2)
        screen_rig.typeline("VTC:userland/bin/env >RAM:se-%s.env" % name, 4)
        screen_rig.typeline("echo T=$TERM C=[$COLORTERM] >RAM:se-%s.vsh" % name, 3)
        env = text("RAM:se-%s.env" % name).splitlines()
        term = [l for l in env if l.startswith("TERM=")]
        ct = [l for l in env if l.startswith("COLORTERM")]
        vs = text("RAM:se-%s.vsh" % name).strip()
        good = True
        if want_term:
            good &= term == [want_term]
        if want_ct is not None:
            good &= (ct == ["COLORTERM=" + want_ct]) if want_ct else (ct in ([], ["COLORTERM="]))
        print("%s %s: env TERM=%r COLORTERM=%r | vsh %r" % ("ok" if good else "FAIL", name, term, ct, vs))
        ok &= good
        screen_rig.typeline("exit", 3)
    rc, out = run("List VTC:terminfo/x/xterm-16color VTC:terminfo/v/vtcon-amiga-16color")
    good = rc == 0 and "xterm-16color" in out and "vtcon-amiga-16color" in out
    print("%s kit terminfo: %s" % ("ok" if good else "FAIL", " ".join(out.split())))
    return 0 if ok and good else 1


if __name__ == "__main__":
    sys.exit(main())
