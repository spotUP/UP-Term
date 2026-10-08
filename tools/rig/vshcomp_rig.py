#!/usr/bin/env python3
"""vshcomp_rig.py -- programmable completion through the console (V93).

A Tab in an argument word of a shell's prompt line (the shell armed the console with
ACTION_VTCON_COMPLETE) answers the shell's waiting Read with the marker line; the line editor keeps
the line and puts in the words the shell sends back. Part 1 drives VTC:compprobe (the console side
alone: the marker and the reply, no vsh); part 2 drives VTC:vsh with `complete -W` and `complete -F`.
Each case types a line with Tabs and Return, and reads what reached RAM:. The rig must be up with the
current handler installed (rig.py install, then a reboot); UPTERM_RIG=2 for rig 2.

  vshcomp_rig.py [probe|vsh]"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, screen_rig

TAB, RET = "\t", "\n"

# compprobe: (its arguments, the keys, what RAM:compprobe.txt must hold)
PROBE = (
    ("one word", "foolish", ["x fo", TAB, "y", RET], "<M>4 x fo\nx foolish y\n"),
    ("several", "foobar foobaz", ["x f", TAB, TAB, "r", RET], "<M>3 x f\nx foobar\n"),
    ("nospace", "-n dir/", ["x d", TAB, "a", RET], "<M>3 x d\nx dir/a\n"),
    ("cursor inside", "foolish", ["x fo zz", 0x4F, 0x4F, 0x4F, TAB, RET], "<M>4 x fo zz\nx foolish  zz\n"),
    ("command word", "foolish", ["zzq", TAB, RET], "zzq\n"),  # the console's own: no marker
)

# vsh: (the keys, the file, what it must hold)
VSH = (
    ("-W words", ["complete -W 'alpha beta bravo' kw", RET, "kw() { echo \"$@\" >RAM:vc1.txt; }", RET,
                  "kw al", TAB, "x", RET], "RAM:vc1.txt", "alpha x"),
    ("-W common, menu", ["kw b", TAB, "r", TAB, RET], "RAM:vc1.txt", "bravo"),
    ("-F function", ["_f() { COMPREPLY=( $(compgen -W 'one two three' -- \"$2\") ); "
                     "echo \"$COMP_CWORD $COMP_LINE\" >RAM:vc3.txt; }", RET,
                     "complete -F _f kw", RET, "kw a t", TAB, RET], "RAM:vc3.txt", "2 kw a t"),
    ("-F result", [], "RAM:vc1.txt", "a t"),
    ("-F inserts", ["kw o", TAB, RET], "RAM:vc1.txt", "one"),
    ("no spec: files", ["echo RAM:vc", TAB, "1.tx", TAB, ">RAM:vc4.txt", RET], "RAM:vc4.txt", "RAM:vc1.txt"),
)


def send(step):
    if step == RET:
        ami.key(0x44)
        time.sleep(2.5)
    elif step == TAB:
        ami.key(0x42)
        time.sleep(1.5)
    elif isinstance(step, int):
        ami.key(step)  # a raw key (0x4F cursor left)
        time.sleep(0.3)
    else:
        ami.req(0x08, bytes([4]) + step.encode())
        time.sleep(0.6)


def window(title):
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/600/200/%s/CLOSE"' % title, 10)
    time.sleep(4)


def probe():
    ok = True
    window("compprobe")
    for name, args, steps, want in PROBE:
        ixpty_rig.run("Delete RAM:compprobe.txt QUIET")
        screen_rig.typeline("VTC:compprobe " + args, 2)
        for s in steps:
            send(s)
        got = ami.read_file("RAM:compprobe.txt") or b""
        got = got.decode("latin-1").replace("\r", "")
        good = got == want
        print("%s: %r %s" % (name, got, "ok" if good else "FAIL (want %r)" % want))
        ok = ok and good
    screen_rig.typeline("endcli", 1)
    return ok


def vsh():
    ok = True
    ixpty_rig.run("Delete RAM:vc1.txt RAM:vc3.txt RAM:vc4.txt QUIET")
    window("vshcomp")
    screen_rig.typeline("VTC:vsh", 4)
    for name, steps, path, want in VSH:
        for s in steps:
            send(s)
        rc, out = ixpty_rig.run("Type %s" % path)
        good = out.strip() == want
        print("%s: %r %s" % (name, out.strip(), "ok" if good else "FAIL (want %r)" % want))
        ok = ok and good
    screen_rig.typeline("exit", 1)
    screen_rig.typeline("endcli", 1)
    return ok


def main():
    ixpty_rig.use_ixemul()
    what = sys.argv[1:] or ["probe", "vsh"]
    ok = True
    if "probe" in what:
        ok = probe() and ok
    if "vsh" in what:
        ok = vsh() and ok
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
