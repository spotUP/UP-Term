#!/usr/bin/env python3
"""ixpty_rig.py -- BSD ptys through the patched ixemul on PTY: (P6 part 4).

Runs on the patched ixemul.library rig.py start put in VTC:ixp6 (IXEMUL=,
default the ixemul-vtcon build; stops when the rig booted another one), puts
that drawer first in LIBS: (the rig's own DH0:Libs keeps the original),
mounts PTY: and runs tests/amiga/ixpty, which checks itself. --orig runs
it on the original library instead (the A/B). The rig must be up;
`make amiga build/amiga/ixpty` first."""
import hashlib, os, pathlib, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import paths
import ptytest_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
# IXEMUL= and IXNET= name others (paths.py): ixnet_open refuses (ix_panic) an
# ixemul of another version or revision, so the two always travel together.
IXEMUL = paths.IXEMUL_LIB
IXNET = paths.IXNET_LIB

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def booted_ixemul():
    """None when VTC:ixp6 holds IXEMUL and IXNET, else the message that says to restart the rig.
    rig.py start copies them there and the boot loads them; vsh and wasabi keep ixemul.library open,
    so Avail FLUSH cannot expunge it and a library copied into VTC:ixp6 later is never loaded:
    a probe would test the old code (s23: sigexit_rc_rig gave rc 0 on the rebuilt build295)."""
    md5 = lambda f: hashlib.md5(pathlib.Path(f).read_bytes()).hexdigest()
    for src, name in ((IXEMUL, "ixemul.library"), (IXNET, "ixnet.library")):
        have = VTC / "ixp6" / name
        if not have.exists() or md5(have) != md5(src):
            return ("the rig did not boot %s (VTC:ixp6/%s differs); restart it with the same IXEMUL= and IXNET="
                    % (src, name))
    return None

def use_ixemul(orig=False):
    """The patched ixemul first in LIBS: for this boot (or the original). Stops the probe when the rig
    booted another ixemul (booted_ixemul)."""
    if not orig:
        why = booted_ixemul()
        if why:
            sys.exit("%s: %s" % (os.path.basename(sys.argv[0]), why))
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
