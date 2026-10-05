#!/usr/bin/env python3
"""ixpty_rig.py -- BSD ptys through the patched ixemul on PTY: (P6 part 4).

Installs the patched ixemul.library (IXEMUL=, default the ixemul-vtcon
build) into VTC:ixp6, puts that drawer first in LIBS: for this boot only
(the rig's own DH0:Libs keeps the original), flushes the old library,
mounts PTY: and runs tests/amiga/ixpty, which checks itself. --orig runs
it on the original library instead (the A/B). The rig must be up;
`make amiga build/amiga/ixpty` first."""
import os, pathlib, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import ptytest_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
IXEMUL = pathlib.Path(os.environ.get("IXEMUL", pathlib.Path.home() /
    "Code/ixemul-vtcon/build295/library/68020/68881/amigaos/ixemul.library"))
# ixnet.library from the same build: ixnet_open refuses (ix_panic) an ixemul
# of another version or revision, so the two always travel together.
IXNET = pathlib.Path(os.environ.get("IXNET", IXEMUL.parents[4] /
    "ixnet/68020/amigaos/ixnet.library"))

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def use_ixemul(orig=False):
    """The patched ixemul first in LIBS: for this boot (or the original)."""
    (VTC / "ixp6").mkdir(exist_ok=True)
    shutil.copyfile(IXEMUL, VTC / "ixp6/ixemul.library")
    shutil.copyfile(IXNET, VTC / "ixp6/ixnet.library")
    run('Avail >NIL: FLUSH')
    if orig:
        run('Assign LIBS: DH0:Libs')
    else:
        run('Assign LIBS: VTC:ixp6')
        run('Assign LIBS: DH0:Libs ADD')
    run('Assign LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD')

def main():
    orig = "--orig" in sys.argv
    for name in ("pty-handler", "ixpty"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    (VTC / "ptymount").write_text(ptytest_rig.MOUNTLIST)
    use_ixemul(orig)
    rc, out = run('Assign >NIL: PTY: EXISTS DEVICES')
    if rc != 0:
        rc, out = run('Mount PTY: FROM VTC:ptymount')
        if rc != 0:
            print("mount failed", rc, out)
            return 2
    rc, out = run('VTC:ixpty', 120)
    sys.stdout.write(out)
    return 0 if rc == 0 and 'FAIL' not in out else 1

if __name__ == '__main__':
    sys.exit(main())
