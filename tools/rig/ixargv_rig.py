#!/usr/bin/env python3
"""ixargv_rig.py -- the argv an ixemul program gets from an AmigaDOS argument line.

ixemul's startup (ixemul-vtcon library/_cli_parse.c, the splitter in
library/cli_args.c) must read the line as dos.library's ReadItem does: double
quotes with the * escapes (*" ** *N *E), the line ending at an unquoted
newline; but a * before any other character stays a star (python3 -c
"print(6*7)", find -name "*.c" typed in the AmigaShell). Before it, vsh's line for printf 'a"b' gave the arguments a* and b",
'c*d' gave c**d, and an argument holding a newline was cut there (V74: a
configure's multi-line sed script; rig 2, 2026-10-08).

Two ways in, both through tests/amiga/ixargv (prints each argument, bytes
outside 33..126 as <hex>):
  - vsh runs it with arguments a bash script means (quote, star, newline,
    empty, plain word, =): vsh writes the line (shell/vsh.c command_line);
  - amiagent's Shell runs a line written by hand with each escape.
readitem (a native ReadArgs command) runs the hand-written line too, as the
reference for the shared cases. Installs IXEMUL= (default the build295 one)
as ixpty_rig does; IXEMUL=<old library> is the A/B. The rig must be up;
`make build/amiga/vsh build/amiga/ixargv build/amiga/readitem` first."""
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
    ("backslash_quote", ['i\\"j']),
    ("newline_with_space", ["s/a/b/\ns/c d/"]),
    ("newline_only", ["p\nq"]),
    ("empty", [""]),
    ("plain", ["plain"]),
    ("equals", ["k=v"]),
    ("mixed", ['a"b', "c*d", "", "plain", "x\ny z"]),
    ("glob_exe", ["*exe", "*n", "*.c"]),
]

# a line as a person types it in the Shell, and the argv it means
DOS_LINE = '"a*"b" "c**d" "x*Ny" "e*Ef" "" plain "q"r a"b'
DOS_WANT = ['a"b', "c*d", "x\ny", "e\x1bf", "", "plain", "q", "r", 'a"b']

# ixemul only, on purpose unlike ReadItem (library/cli_args.c): in quotes a *
# before anything but " * N n E e is a literal star, so a Unix user's quoted
# glob or expression typed in the AmigaShell arrives whole (ReadItem: print(67), .c)
STAR_LINE = '-c "print(6*7)" "*.c" "a*b" "[*]" "*exe" "**exe"'
STAR_WANT = ["-c", "print(6*7)", "*.c", "a*b", "[*]", "\x1bxe", "*exe"]

def sq(a):
    return "'" + a.replace("'", "'\\''") + "'"

def main():
    for name in ("vsh", "ixargv", "readitem"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    ixpty_rig.use_ixemul()
    ok = True
    script = "".join("echo '== %s'\nVTC:ixargv %s\n" % (n, " ".join(sq(a) for a in args))
                     for n, args in VSH_CASES)
    (VTC / "ixargv.sh").write_bytes(script.encode("latin-1"))
    rc, out = ixpty_rig.run("VTC:vsh VTC:ixargv.sh", 120)
    parts = out.split("== ")[1:]
    got = {p.split("\n", 1)[0]: p.split("\n", 1)[1] for p in parts}
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
