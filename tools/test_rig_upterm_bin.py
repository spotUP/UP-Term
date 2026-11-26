"""tools/rig/rig.py: the clone's UP-Term:bin is made current before the boot, and a stale drawer fails the start.

Regression (RIG-BLACK-SCREEN / RIG-START-SYNC, rig 21, 2026-11-25): the master image's UP-Term:bin held a
patina-flat from before PENS. The kit's PENS block in S:User-Startup ran it without Run, the old binary became the
resident on the boot CLI, C:LoadWB never ran, and the RTG rig had no screen open. The staleness check after the
boot only warned.
"""
import pathlib
import subprocess
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


def fake_xdftool(disk, writes):
    """subprocess.run over a dict model of one partition: list, read, write, delete; '+' chains like xdftool."""
    def run(argv, check=False, **kw):
        a = argv[argv.index("+") + 1:]
        ops, cur = [], []
        for w in a + ["+"]:
            if w == "+":
                ops.append(cur)
                cur = []
            else:
                cur.append(w)
        out, rc = "", 0
        for op in ops:
            if op[0] == "list":
                names = [k.split("/")[-1] for k in disk if k.lower().startswith(op[1].lower() + "/")]
                rc = 0 if names else 10
                out = "".join("%s  100\n" % n for n in names)
            elif op[0] == "read":
                if op[1] in disk:
                    pathlib.Path(op[2]).write_bytes(disk[op[1]])
            elif op[0] == "delete":
                disk.pop(op[1], None)
            elif op[0] == "write":
                disk[op[2]] = pathlib.Path(op[1]).read_bytes()
                writes.append(op[2])
        if check and rc:
            raise subprocess.CalledProcessError(rc, argv)
        return subprocess.CompletedProcess(argv, rc, out, "")
    return run


with tempfile.TemporaryDirectory() as t:
    root = pathlib.Path(t)
    built = root / "build/amiga"
    built.mkdir(parents=True)
    (built / "patina-flat").write_bytes(b"new flat with PENS")
    (built / "patina-gui").write_bytes(b"gui")
    (built / "vsh").write_bytes(b"vsh, not in the drawer")
    disk = {"Tools/UP-Term/bin/patina-flat": b"old flat", "Tools/UP-Term/bin/patina-gui": b"gui",
            "Tools/UP-Term/bin/README": b"kit file"}
    writes, made = [], []
    saved = rig.ROOT, rig.RIG, rig.clone_upterm_place, rig.ensure_built, rig.subprocess.run
    rig.ROOT, rig.RIG = root, root / "rig"
    rig.clone_upterm_place = lambda image: ("DH1", "Tools/UP-Term")
    rig.ensure_built = lambda files, run=None: made.extend(pathlib.Path(f).name for f in files) or []
    rig.subprocess.run = fake_xdftool(disk, writes)
    try:
        put = rig.sys_hdf_upterm_bin(root / "sys.hdf")
        check(put == ["patina-flat"], "only the stale drawer file is put: %r" % put)
        check(disk["Tools/UP-Term/bin/patina-flat"] == b"new flat with PENS", "the clone's patina-flat is the build's")
        check(sorted(made) == ["patina-flat", "patina-gui"], "every held build file is made current first: %r" % made)
        check("Tools/UP-Term/bin/vsh" not in disk, "a build file the drawer does not hold is not added")
        check(disk["Tools/UP-Term/bin/README"] == b"kit file", "a non-build drawer file is untouched")
        writes.clear()
        check(rig.sys_hdf_upterm_bin(root / "sys.hdf") == [] and not writes, "a current drawer gets no write")
        check(not (root / "rig/upterm-bin.tmp").exists(), "the read-back scratch is removed")
        rig.clone_upterm_place = lambda image: None
        check(rig.sys_hdf_upterm_bin(root / "sys.hdf") == [], "a clone with no UP-Term: is left alone")
    finally:
        rig.ROOT, rig.RIG, rig.clone_upterm_place, rig.ensure_built, rig.subprocess.run = saved

# the start runs it before the boot on both clone paths (fresh and --keep-sys), before the PENS block is written
src = (HERE / "rig/rig.py").read_text()
start = src[src.index("def _start(retries):"):]
start = start[:start.index("RIG.mkdir(parents=True, exist_ok=True)")]
check(start.count('sys_hdf_upterm_bin(RIG / "sys.hdf")') == 2, "both clone paths of _start make UP-Term:bin current")
for part in start.split("elif not OS and not OS32:"):
    check(part.index("sys_hdf_upterm_bin(") < part.index("sys_hdf_shadows_block("), "the sync runs before the PENS block")

# after the boot: stale fails the start, a failed look still only warns
saved = rig.upterm_sync
try:
    rig.upterm_sync = lambda check=False: ["patina-flat"]
    try:
        rig.require_upterm_current()
        check(False, "a stale UP-Term:bin fails the start")
    except SystemExit as e:
        check("patina-flat" in str(e) and "upterm-sync" in str(e), "the refusal names the file and the fix: %s" % e)
    rig.upterm_sync = lambda check=False: []
    rig.require_upterm_current()
    def broken(check=False):
        raise OSError("no answer")
    rig.upterm_sync = broken
    rig.require_upterm_current()
finally:
    rig.upterm_sync = saved
check("require_upterm_current()" in src and "warn_upterm_stale" not in src, "the boot wait calls the failing check")

print("FAIL" if fails else "PASS", "test_rig_upterm_bin")
sys.exit(1 if fails else 0)
