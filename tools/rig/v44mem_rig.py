#!/usr/bin/env python3
"""v44mem_rig.py -- V44's array memory on the rig (plan 2026-10-07-vsh-bash, V44), measured the way the
item defines its budget: a 10,000-element indexed array `a[i]=x$i` costs at most 24 bytes per element
beyond the strings' own bytes (each "x<i>" with its NUL), and after `unset a` a second fill/unset cycle
keeps 0 bytes.

One run: vsh (VSH=<binary>, default build/amiga/vsh, copied to VTC:v44mem/vsh) runs a script that takes
`C:Avail FLUSH TOTAL` before the fill (m0), after it (m1), after `unset a` (m2) and after a second fill
and unset (m3). Avail writes to RAM: files made before m0, so no measurement adds a file node.

Not with `$(C:Avail ...)`: a command substitution was a subshell process holding a copy of the shell's
variables (sh_shell_clone), so the array was counted twice while Avail ran: 53.95 bytes per element
on rig 2 for the same build that takes 27.9 here (2026-10-08). Since the shared table it holds none
(tools/rig/substmem_rig.py), but the file keeps the measurement independent of that.

Each run's numbers go to build/<rig>/userland/v44mem/<vsh sha1>.log as it ends; the next call goes on
from the recorded runs (--runs N in total, --fresh starts again). Exit 0 when every recorded run of this
vsh is in budget.

  UPTERM_RIG=2 python3 tools/rig/v44mem_rig.py              3 runs of build/amiga/vsh
  UPTERM_RIG=2 python3 tools/rig/v44mem_rig.py --runs 5     2 more after those 3

The rig must be up; UPTERM_RIG selects which one."""
import argparse, hashlib, os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import userland_rig as U

N = 10000
BUDGET = 24.0
STRING_BYTES = sum(len("x%d" % i) + 1 for i in range(N))   # the strings' own bytes, NULs counted
LEDGER = paths.RIG / "userland" / "v44mem"
SCRIPT = """\
for k in 0 1 2 3; do echo 0000000000 >RAM:v44m$k; done
C:Avail FLUSH TOTAL >RAM:v44m0
for ((i=0;i<%(n)d;i++)); do a[i]=x$i; done
C:Avail FLUSH TOTAL >RAM:v44m1
unset a
C:Avail FLUSH TOTAL >RAM:v44m2
for ((i=0;i<%(n)d;i++)); do a[i]=x$i; done
unset a
C:Avail FLUSH TOTAL >RAM:v44m3
read m0 <RAM:v44m0; read m1 <RAM:v44m1; read m2 <RAM:v44m2; read m3 <RAM:v44m3
echo "$m0 $m1 $m2 $m3"
""" % {"n": N}


def stage(vsh):
    d = U.VTC / "v44mem"
    d.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(vsh, d / "vsh")
    (d / "v44mem.sh").write_text(SCRIPT, encoding="latin-1")


def one_run():
    """(m0, m1, m2, m3), the free bytes Avail gave at the four points"""
    rc, out = U.run("VTC:v44mem/vsh VTC:v44mem/v44mem.sh", 900)
    nums = out.split()
    if rc != 0 or len(nums) != 4:
        raise SystemExit("[ERROR] the measurement did not run: rc %d, output %r" % (rc, out))
    return tuple(int(x) for x in nums)


def verdict(m):
    m0, m1, m2, m3 = m
    beyond = ((m0 - m1) - STRING_BYTES) / N
    first_kept, second_kept = m0 - m2, m2 - m3
    ok = beyond <= BUDGET and second_kept == 0
    return ("%s per-element %.2f beyond-string %.2f first-unset-kept %d second-cycle-kept %d avail %d %d %d %d"
            % ("PASS" if ok else "FAIL", (m0 - m1) / N, beyond, first_kept, second_kept, m0, m1, m2, m3))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--runs", type=int, default=3, help="runs recorded in total for this vsh")
    ap.add_argument("--fresh", action="store_true", help="forget this vsh's recorded runs")
    a = ap.parse_args()
    vsh = os.environ.get("VSH") or str(U.ROOT / "build/amiga/vsh")
    sha = hashlib.sha1(open(vsh, "rb").read()).hexdigest()[:12]
    LEDGER.mkdir(parents=True, exist_ok=True)
    log = LEDGER / (sha + ".log")
    if a.fresh:
        log.unlink(missing_ok=True)
    done = log.read_text().splitlines() if log.exists() else []
    if len(done) < a.runs:
        stage(vsh)
    for i in range(len(done) + 1, a.runs + 1):
        line = "run %d %s" % (i, verdict(one_run()))
        with log.open("a") as f:
            f.write(line + "\n")
        print(line, flush=True)
    lines = log.read_text().splitlines()
    good = sum(" PASS " in l for l in lines)
    print("v44mem %s: %d of %d recorded runs in budget (%s; budget %.0f bytes per element beyond the strings' "
          "%d bytes, second cycle 0)" % (sha, good, len(lines), log, BUDGET, STRING_BYTES))
    return 0 if lines and good == len(lines) else 1


if __name__ == "__main__":
    sys.exit(main())
