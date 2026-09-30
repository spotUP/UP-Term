#!/usr/bin/env python3
"""ixpipe_rig.py -- a pipe from an ixemul program into a vfork + exec
child (tests/amiga/ixpipeprobe; GNU screen's printcmd does this):

  1. IXPIPE: not mounted (a fresh boot: the rig's boot does not mount it):
     a native child (vsh -c) must not stop on an "insert volume IXPIPE:"
     requester (patched ixemul, stdlib/execve.c dup2_BPTR).
  2. IXPIPE: mounted: the line arrives in an ixemul child (A), in an
     ixemul program behind vsh -c (B: vsh used to hand it its console
     instead of the pipe) and in vsh itself (C).

Rig up with a fresh boot for part 1 (it is skipped, and says so, when
IXPIPE: is already mounted)."""
import os, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, screen_rig
from install_rig import run, check, ROOT
import install_rig

VTC = ROOT / "build/rig/vtc"


def main():
    for name in ("ixpipeprobe", "forkprobe", "vsh", "ixpipe-handler"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    (VTC / "ixpipemount").write_text(screen_rig.IXPIPE_MOUNT)
    ixpty_rig.use_ixemul()
    if run('Assign >NIL: IXPIPE: EXISTS DEVICES')[0] != 0:
        ami.req(0x02, struct.pack('>H', 10) + b'Run >NIL: VTC:ixpipeprobe B')
        time.sleep(6)
        req = 'System Request' in ami.req(0x0D).decode('latin-1')
        check(not req, 'IXPIPE: not mounted: no requester for a native child')
        if req:
            ami.main(['ui', 'click', 'System Request', 'Cancel'])
        run('Mount IXPIPE: FROM VTC:ixpipemount')
    else:
        print('skip: IXPIPE: is mounted already (part 1 needs a fresh boot)')
    run('Delete RAM:ixpipeprobe.log RAM:forkprobe.log QUIET')
    rc, out = run('VTC:ixpipeprobe ABC', 60)
    log, probe = run('Type RAM:ixpipeprobe.log')[1], run('Type RAM:forkprobe.log')[1]
    check(rc == 0, 'every child ends', log)
    check(probe.count('stdin=printed') == 2, 'A and B: the ixemul program reads the line', probe)
    check('got=printed' in log, 'C: vsh reads the line', log)
    print('ixpipe_rig: passed %d of %d' % (install_rig.passed, install_rig.total))
    return 0 if install_rig.passed == install_rig.total else 1


if __name__ == '__main__':
    sys.exit(main())
