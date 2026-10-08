#!/usr/bin/env python3
"""substmem_rig.py -- what a command substitution costs in memory while it runs, on the rig: with a
10,000-element indexed array `a[i]=x$i` in the shell, `$(C:Avail FLUSH TOTAL)` may take at most 1 byte
per element more than it takes without the array (budget). vsh has no fork: a subshell that copied every
variable (sh_shell_clone before the shared table, 2026-10-08) took about 24 bytes per element more.

One run: vsh (VSH=<binary>, default build/amiga/vsh, copied to VTC:substmem/vsh) runs a script that takes
`C:Avail FLUSH TOTAL` into a RAM: file (f0) and through `$( )` (s0) before the fill, and the same two after it
(f1, s1). f - s is what the substitution holds while Avail runs; the array's share of it is
((f1 - s1) - (f0 - s0)) / 10,000 bytes per element.

Each run's numbers go to build/<rig>/userland/substmem/<vsh sha1>.log as it ends; the next call goes on
from the recorded runs (--runs N in total, --fresh starts again). Exit 0 when every recorded run of this
vsh is in budget.

  UPTERM_RIG=2 python3 tools/rig/substmem_rig.py              3 runs of build/amiga/vsh
  VSH=old/vsh UPTERM_RIG=2 python3 tools/rig/substmem_rig.py  another binary

The rig must be up; UPTERM_RIG selects which one."""
import argparse, hashlib, os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import userland_rig as U

N = 10000
BUDGET = 1.0
LEDGER = paths.RIG / "userland" / "substmem"
SCRIPT = """\
for k in 0 1; do echo 0000000000 >RAM:smem$k; done
s0=0000000000 s1=0000000000
C:Avail FLUSH TOTAL >RAM:smem0
s0=$(C:Avail FLUSH TOTAL)
for ((i=0;i<%(n)d;i++)); do a[i]=x$i; done
C:Avail FLUSH TOTAL >RAM:smem1
s1=$(C:Avail FLUSH TOTAL)
read f0 <RAM:smem0; read f1 <RAM:smem1
echo "$f0 $s0 $f1 $s1"
""" % {"n": N}


def stage(vsh):
    d = U.VTC / "substmem"
    d.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(vsh, d / "vsh")
    (d / "substmem.sh").write_text(SCRIPT, encoding="latin-1")


def one_run():
    """(f0, s0, f1, s1), the free bytes Avail gave"""
    rc, out = U.run("VTC:substmem/vsh VTC:substmem/substmem.sh", 900)
    nums = out.split()
    if rc != 0 or len(nums) != 4:
        raise SystemExit("[ERROR] the measurement did not run: rc %d, output %r" % (rc, out))
    return tuple(int(x) for x in nums)


def verdict(m):
    f0, s0, f1, s1 = m
    per = ((f1 - s1) - (f0 - s0)) / N
    return ("%s in-flight-per-element %.2f without-array %d with-array %d avail %d %d %d %d"
            % ("PASS" if per <= BUDGET else "FAIL", per, f0 - s0, f1 - s1, f0, s0, f1, s1))


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
    print("substmem %s: %d of %d recorded runs in budget (%s; budget %.0f byte per element while $( ) runs)"
          % (sha, good, len(lines), log, BUDGET))
    return 0 if lines and good == len(lines) else 1


if __name__ == "__main__":
    sys.exit(main())
