"""Makefile: a new git revision rebuilds vtcon-handler and pty-handler, so the
"UP-Term build <rev>" in their $VER names the commit the kit was built from.

Regression (2026-10-22): after `make dist` at e5747ab4 with uncommitted
changes, two more commits and a clean tree left build/amiga/obj/handler.o
saying "vtcon-handler 1.7 (22.10.2026) e5747ab4_dirty". The handler and the
pty handler are forced to rebuild only when handler.flags / pty.flags differ
from the flags of this make, and the revision was not one of the flags.

Each check runs `make -n` against a scratch BUILD directory whose binaries are
newer than every source (dated 2099) and whose version header make is told is
up to date (-o), so the flags file is the only reason left to rebuild.
"""
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
FUTURE = 4102444800  # 2100-01-01, newer than any source file
fails = 0

# target, its flags file, a fragment of its compile line
UNITS = [
    ("vtcon-handler", "handler.flags", "handler.o handler/vtcon_handler.c"),
    ("pty-handler", "pty.flags", "pty_handler.o handler/pty_handler.c"),
]


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


def dry(build, target, rev):
    """make -n for build/amiga/<target> as if HEAD were rev."""
    hdr = "%s/amiga/upterm_version.h" % build
    r = subprocess.run(["make", "-n", "BUILD=%s" % build, "GITREV=%s" % rev, "-o", hdr,
                        "%s/amiga/%s" % (build, target)],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("make -n %s failed: %s" % (target, r.stderr.strip()[-400:]))
    return r.stdout


with tempfile.TemporaryDirectory() as d:
    build = os.path.join(d, "build")
    amiga = pathlib.Path(build, "amiga")
    amiga.mkdir(parents=True)
    for name in ["upterm_version.h"] + [u[0] for u in UNITS]:
        p = amiga / name
        p.write_text("")
        os.utime(p, (FUTURE, FUTURE))
    for target, flagfile, compile_line in UNITS:
        # the flags a build at revision 1111111 records (its own "echo ... > flags" line)
        rec = next((l for l in dry(build, target, "1111111").splitlines()
                    if l.startswith("echo ") and l.rstrip().endswith(flagfile)), None)
        check(rec is not None, "%s: no flags line in make -n" % target)
        if rec is None:
            continue
        (amiga / flagfile).write_text(rec[len("echo '"):rec.rindex("'")] + "\n")
        same = dry(build, target, "1111111")
        check(compile_line not in same, "%s rebuilds at the revision it was built at" % target)
        other = dry(build, target, "2222222")
        check(compile_line in other, "%s is not rebuilt for a new git revision (stale build stamp)" % target)

if fails:
    sys.exit("%d failed" % fails)
print("[OK] build_stamp: %d checks, 0 failed" % (3 * len(UNITS)))
