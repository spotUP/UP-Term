"""rig.py fresh_sys_hdf: a rig number that never ran (no build/rig<N> yet) gets its sys.hdf clone.

2026-10-28 (RIG-CLONE-NEW-NUMBER): `UPTERM_RIG=11 rig.py start --fresh` stopped with "cp -c: clonefile failed:
No such file or directory", because start() clones the system disk into the rig dir before it creates that dir.
No FS-UAE is run here: a small master image in a scratch dir is cloned into a rig dir that does not exist yet."""
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import rig  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


with tempfile.TemporaryDirectory(dir=HERE.parent / "build") as tmp:
    tmp = pathlib.Path(tmp)
    master = tmp / "master.hdf"
    master.write_bytes(b"DOS\x03" + b"\0" * 1020)
    rig.SRC_HDF = master
    rig.RIG = tmp / "rig99"   # never made: a rig number that never ran
    try:
        rig.fresh_sys_hdf()
    except SystemExit as e:
        check(False, "fresh_sys_hdf refused a new rig number: %s" % e)
    clone = rig.RIG / "sys.hdf"
    check(clone.exists() and clone.read_bytes() == master.read_bytes(), "the new rig dir holds a clone of the master")
    check(not (rig.RIG / "sys.hdf.new").exists(), "no half-written sys.hdf.new left behind")

print("FAIL" if fails else "ok", "test_rig_clone_new")
sys.exit(1 if fails else 0)
