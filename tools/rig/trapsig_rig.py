#!/usr/bin/env python3
"""trapsig_rig.py -- V78 on the rig: which signals reach a vsh script's traps on the Amiga.

A vsh (VSH=<binary>, default build/amiga/vsh, copied to VTC:trapsig/vsh) runs, started with Run, a script
that sets traps on HUP INT QUIT TERM USR1 USR2 WINCH TSTP (each appends its name to RAM:trapsig.log) and
loops for up to 120 s. The rig then sends, one at a time, `kill -s SIG <pid>` from a second vsh for INT,
QUIT, TERM, HUP, USR1, and the AmigaDOS `Break <process> D` and `F`, and records which trap ran.

Must hold: INT runs the INT trap (Ctrl-C) and QUIT the QUIT trap (break bit E, which vsh's kill sends for
QUIT: before 2026-10-08 vsh polled Ctrl-C only and QUIT never arrived). The rest is printed as found:
TERM and HUP reach a native task as Ctrl-C (os_signal's fallback when ixkill cannot signal it), USR1 cannot
be sent to a native task, D and F map to no signal (dist/README.txt, tests/bash/divergences.txt).

  UPTERM_RIG=2 python3 tools/rig/trapsig_rig.py

The rig must be up."""
import os, re, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import userland_rig as U

SCRIPT = """\
echo $$ >RAM:trapsig.pid
: >RAM:trapsig.log
for s in HUP INT QUIT TERM USR1 USR2 WINCH TSTP; do trap "echo $s >>RAM:trapsig.log" $s; done
trap 'echo EXIT >>RAM:trapsig.log' EXIT
while [ $SECONDS -lt 120 ]; do [ -e RAM:trapsig.stop ] && exit 0; done
"""
MUST = {"INT": "INT", "QUIT": "QUIT"}


def log_lines():
    rc, out = U.run("Type RAM:trapsig.log", 30)
    return [l.strip() for l in out.splitlines() if l.strip()]


def main():
    vsh = os.environ.get("VSH") or str(U.ROOT / "build/amiga/vsh")
    d = U.VTC / "trapsig"
    d.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(vsh, d / "vsh")
    (d / "trapsig.sh").write_text(SCRIPT, encoding="latin-1")
    U.run("Delete RAM:trapsig.stop RAM:trapsig.log RAM:trapsig.pid QUIET", 30)
    U.run("Run >NIL: VTC:trapsig/vsh VTC:trapsig/trapsig.sh", 30)
    time.sleep(3)
    pid = U.run("Type RAM:trapsig.pid", 30)[1].strip()
    status = U.run("Status", 30)[1]
    m = re.search(r"Process\s+(\d+): Loaded as command: VTC:trapsig/vsh", status)
    proc = m.group(1) if m else None
    seen, ok = len(log_lines()), True
    sends = [(s, 'VTC:trapsig/vsh -c "kill -s %s %s"' % (s, pid)) for s in ("INT", "QUIT", "TERM", "HUP", "USR1")]
    sends += [("break-" + b, "Break %s %s" % (proc, b)) for b in ("D", "F")] if proc else []
    for name, cmd in sends:
        rc, out = U.run(cmd, 60)
        time.sleep(2)
        lines = log_lines()
        new = lines[seen:]
        seen = len(lines)
        got = " ".join(new) or "nothing"
        want = MUST.get(name)
        good = want is None or new == [want]
        ok = ok and good
        print("%s %-8s -> %s%s" % ("ok  " if good else "FAIL", name, got,
                                   "" if not out.strip() or rc == 0 else "  (sender: %s)" % out.strip().splitlines()[-1]),
              flush=True)
    U.run("Echo >RAM:trapsig.stop x", 30)
    time.sleep(3)
    print("trapsig_rig: %s (process %s, pid %s)" % ("PASS" if ok else "FAIL", proc, pid))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
