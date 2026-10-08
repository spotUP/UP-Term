#!/usr/bin/env python3
"""ixargv_rig.py -- the argv an ixemul program gets, from vsh and from the AmigaShell.

vsh gives an ixemul program its argv out of band, after the line's newline
(vtcon shell/sh_ixargv.h; ixemul-vtcon library/cli_args.c reads it): every
byte as bash means it, stars, quotes, newlines and empty arguments included.
A line a person types in the AmigaShell is split as ReadItem splits it, but
inside quotes * is an ordinary character (no *" ** *N *E escapes), so
python3 -c "print(2**3)" prints 8 and grep "error.*" keeps its star.

Three ways in, all through tests/amiga/ixargv (prints each argument, bytes
outside 33..126 as <hex>):
  - vsh runs it with arguments a bash script means (quote, stars, newline,
    empty, plain word, =, a lone quote);
  - amiagent's Shell runs lines written by hand: one ReadItem reads the same
    way (no star inside quotes), one with stars in quotes;
  - readitem (a native ReadArgs command) runs the first hand-written line, as
    the reference for the shared rules;
  - vsh starts it through a Shell: as a Resident command (ixargvr) and as the
    command of AmigaDOS Run; the argv then travels in the local variable
    __ixargv (Run's output: IXARGV_OUT=RAM:ixrun.out).
It also checks that vsh gives a loaded command its drawer as PROGDIR: (C:List
finds itself) and that python3 (VTC:Python3, when the rig has it) finds its
library from vsh with no PYTHONHOME. VSH=<binary> runs another vsh.
Installs IXEMUL= (default the build295 one) as ixpty_rig does; IXEMUL=<old
library> is the A/B. The rig must be up; `make build/amiga/vsh
build/amiga/ixargv build/amiga/readitem` first."""
import os, pathlib, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"

def show(s):
    return "".join(c if 32 < ord(c) < 127 and c not in "<>" else "<%02x>" % ord(c) for c in s)

def want(args):
    return "argc %d\n" % len(args) + "".join("arg [%s]\n" % show(a) for a in args)

# (name, the arguments as bash means them, written in bash single quotes)
VSH_CASES = [
    ("quote", ['a"b']),
    ("star", ["c*d"]),
    ("power", ["a**b"]),
    ("star_quote", ['c*"d']),
    ("lone_quote", ['"']),
    ("backslash_quote", ['i\\"j']),
    ("newline_with_space", ["s/a/b/\ns/c d/"]),
    ("newline_only", ["\n"]),
    ("newline_inside", ["p\nq"]),
    ("empty", [""]),
    ("plain", ["plain"]),
    ("equals", ["k=v"]),
    ("mixed", ['a"b', "c*d", "", "plain", "x\ny z", "a**b", '"']),
    ("glob_exe", ["*exe", "*n", "*.c", "?x", "#?"]),
]

# a line as a person types it in the Shell, and the argv it means; ReadArgs
# reads it the same way (no * inside quotes)
DOS_LINE = '"" plain "q"r a"b "x y"'
DOS_WANT = ["", "plain", "q", "r", 'a"b', "x y"]

# the arguments for the ixemul programs a Shell starts for vsh: a Resident one
# (ixargvr, made resident from VTC:ixargv) and the command of AmigaDOS Run
SHELL_ARGS = ["a**b", 'c*"d', '"', "\n", "", "x y", "k=v", "*.c", "#?", "p\nq"]

# a native command vsh runs right after the ixemul ones: it still gets the
# AmigaDOS line (ReadArgs reads ** as *, *" as "), and no argv meant for another
NATIVE_ARGS = ["a**b", 'c*"d', "x y"]

# ixemul only, on purpose unlike ReadItem (library/cli_args.c): inside quotes
# * is an ordinary character, so a Unix user's quoted expression, regex or
# glob typed in the AmigaShell arrives whole (ReadItem: 2*3, print(67), .c)
STAR_LINE = ('-c "print(2**3)" "print(6*7)" "*.c" "a*b" "[*]" "*exe" "**exe" "*" "error.*" '
             '"*name*" "x*Ny" "a*"b')
STAR_WANT = ["-c", "print(2**3)", "print(6*7)", "*.c", "a*b", "[*]", "*exe", "**exe", "*", "error.*",
             "*name*", "x*Ny", "a*", "b"]

# python3 on the rig (VTC:Python3, a copy of the kit's drawer)
PYTHON = (VTC / "Python3/bin/python3").exists()

def sq(a):
    return "'" + a.replace("'", "'\\''") + "'"

def report(name, good, want_s, got_s):
    print("%s %s" % ("PASS" if good else "FAIL", name))
    if not good:
        sys.stdout.write("  want " + want_s.replace("\n", " | ") + "\n  got  " + got_s.replace("\n", " | ") + "\n")
    return good

def main():
    for name in ("ixargv", "readitem"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    # VSH=: another vsh binary (the A/B)
    shutil.copyfile(os.environ.get("VSH") or ROOT / "build/amiga/vsh", VTC / "vsh")
    ixpty_rig.use_ixemul()
    ok = True
    ixpty_rig.run("Resident >NIL: ixargvr REMOVE\nResident ixargvr VTC:ixargv PURE\nDelete >NIL: RAM:ixrun.out", 60)
    script = "".join("echo '== %s'\nVTC:ixargv %s\n" % (n, " ".join(sq(a) for a in args))
                     for n, args in VSH_CASES)
    shell_args = " ".join(sq(a) for a in SHELL_ARGS)
    script += "echo '== resident'\nixargvr %s\n" % shell_args
    # a command's program directory (PROGDIR:) is the drawer vsh loaded it from, as the Shell gives
    # it (C:List finds itself there); python3 finds its library from it, with no PYTHONHOME
    script += "echo '== progdir'\nC:List PROGDIR:List LFORMAT %n\n"
    if PYTHON:
        script += "echo '== python'\nunset PYTHONHOME\nVTC:Python3/bin/python3 -c 'print(2**3)'\n"
    script += "echo '== run'\nIXARGV_OUT=RAM:ixrun.out run VTC:ixargv %s\n" % shell_args
    script += "echo '== native'\nVTC:readitem %s\n" % " ".join(sq(a) for a in NATIVE_ARGS)
    (VTC / "ixargv.sh").write_bytes(script.encode("latin-1"))
    rc, out = ixpty_rig.run("Stack 1000000\nVTC:vsh VTC:ixargv.sh", 300)
    parts = out.split("== ")[1:]
    got = {p.split("\n", 1)[0]: p.split("\n", 1)[1] for p in parts}
    ref = [l[5:-1].lower() for l in got.get("native", "").splitlines() if l.startswith("arg [")]
    good = ref == [show(a).lower() for a in NATIVE_ARGS]
    ok = ok and good
    print("%s vsh native readargs %s" % ("PASS" if good else "FAIL", ref))
    ok = report("vsh resident ixemul command", got.get("resident") == want(SHELL_ARGS),
                want(SHELL_ARGS), got.get("resident") or "(none)") and ok
    ok = report("vsh progdir", got.get("progdir") == "List\n", "List\n", got.get("progdir") or "(none)") and ok
    if PYTHON:
        ok = report("vsh python3 finds its library", got.get("python") == "8\n", "8\n",
                    got.get("python") or "(none)") and ok
    rc, out = ixpty_rig.run("Wait 3\nType RAM:ixrun.out\nResident >NIL: ixargvr REMOVE", 60)
    ok = report("vsh run ixemul command", out == want(SHELL_ARGS), want(SHELL_ARGS), out) and ok
    for n, args in VSH_CASES:
        good = got.get(n) == want(args)
        ok = ok and good
        print("%s vsh %s" % ("PASS" if good else "FAIL", n))
        if not good:
            sys.stdout.write("  want " + want(args).replace("\n", " | ") + "\n  got  "
                             + (got.get(n) or "(none)").replace("\n", " | ") + "\n")
    rc, out = ixpty_rig.run("VTC:ixargv " + DOS_LINE, 60)
    good = out == want(DOS_WANT)
    ok = ok and good
    print("%s dos line" % ("PASS" if good else "FAIL"))
    if not good:
        sys.stdout.write("  want " + want(DOS_WANT).replace("\n", " | ") + "\n  got  " + out.replace("\n", " | ") + "\n")
    rc, out = ixpty_rig.run("VTC:ixargv " + STAR_LINE, 60)
    good = out == want(STAR_WANT)
    ok = ok and good
    print("%s star line" % ("PASS" if good else "FAIL"))
    if not good:
        sys.stdout.write("  want " + want(STAR_WANT).replace("\n", " | ") + "\n  got  " + out.replace("\n", " | ") + "\n")
    # the reference: ReadArgs reads the same line to the same arguments
    rc, out = ixpty_rig.run("VTC:readitem " + DOS_LINE, 60)
    ref = [l[5:-1].lower() for l in out.splitlines() if l.startswith("arg [")]
    good = ref == [show(a).lower() for a in DOS_WANT]
    ok = ok and good
    print("%s readargs reference %s" % ("PASS" if good else "FAIL", ref))
    print("ixargv_rig: %s (ixemul %s)" % ("OK" if ok else "FAIL", ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
