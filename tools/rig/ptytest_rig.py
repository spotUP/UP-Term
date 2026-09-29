#!/usr/bin/env python3
"""ptytest_rig.py -- PTY: on the rig (P5): installs pty-handler and
tests/amiga/ptytest into VTC:, mounts PTY: and runs the probe, which
checks itself; prints its lines and exits non-zero on a FAIL. The rig
must be up; `make amiga` first."""
import os, pathlib, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
MOUNTLIST = """PTY:
   Handler   = VTC:pty-handler
   Priority  = 5
   StackSize = 8000
   GlobVec   = -1
#
"""

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def main():
    for name in ("pty-handler", "ptytest"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    (VTC / "ptymount").write_text(MOUNTLIST)
    rc, out = run('Assign >NIL: PTY: EXISTS DEVICES')
    if rc != 0:
        rc, out = run('Mount PTY: FROM VTC:ptymount')
        if rc != 0:
            print("mount failed", rc, out)
            return 2
    rc, out = run('VTC:ptytest', 120)
    sys.stdout.write(out)
    return 0 if rc == 0 and 'FAIL' not in out else 1

if __name__ == '__main__':
    sys.exit(main())
