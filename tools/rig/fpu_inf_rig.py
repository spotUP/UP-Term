#!/usr/bin/env python3
"""fpu_inf_rig.py -- the emulated FPU handles Inf and NaN like a real 68881/68060.

FS-UAE 3.1.66 on arm64 (native host-double FPU, no long double) stored fmove.x of +Inf as
3fff0000 ffffffff ffffffff and set NAN, not Z, for fcmp Inf,Inf.  ixemul's compiled Inf==Inf then
returned false (22 of 182 libm_num rows).  tests/amiga/fpuinf.c runs the raw instructions and
asserts: fmove.x +Inf / -Inf bytes, Z (not NAN) after fcmp Inf,Inf, and NaN stays unordered.
FAILS on the stock FS-UAE.app, passes on the patched build (branch fix/arm64-fpu-inf):

  make build/amiga/fpuinf
  UPTERM_FSUAE=<patched fs-uae binary> UPTERM_RIG=2 python3 tools/rig/rig.py start --max
  UPTERM_RIG=2 python3 tools/rig/fpu_inf_rig.py"""
import os, pathlib, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"


def main():
    shutil.copyfile(ROOT / "build/amiga/fpuinf", VTC / "fpuinf")
    rc, out = ixpty_rig.run("VTC:fpuinf", 60)
    out = out.replace("\r", "")
    sys.stdout.write(out)
    ok = rc == 0 and "[OK] fpuinf" in out and "FAIL" not in out
    print("fpu_inf_rig: %s (emulator %s)" % ("OK" if ok else "FAIL", paths.FSUAE))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
