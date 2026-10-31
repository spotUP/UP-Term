"""rig.py clear removes build/rig<N> only when that rig's fs-uae is not running (RIG-FOLDER-VANISHED: rig 9 ran with its
folder deleted under it, rig 4's folder vanished). Fails with the mine() guard removed from clear()."""
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
fails = 0


def run(running, rigdir):
    code = ("import sys,pathlib; sys.path.insert(0,%r); sys.argv=['rig.py']; import rig\n"
            "rig.RIG=pathlib.Path(%r); rig.mine=lambda: %r\n"
            "sys.exit(rig.main(['rig.py','clear']))\n" % (str(HERE / "rig"), str(rigdir), ["fs-uae"] if running else []))
    return subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True)


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


base = pathlib.Path(tempfile.mkdtemp())
rd = base / "rig9"
(rd / "vtc").mkdir(parents=True)
(rd / "vtc" / "f").write_text("x")
r = run(True, rd)
check(r.returncode != 0 and "running" in (r.stderr + r.stdout), "refused while fs-uae runs: rc=%s %r" % (r.returncode, r.stderr[-200:]))
check((rd / "vtc" / "f").exists(), "folder untouched while running")
r = run(False, rd)
check(r.returncode == 0 and not rd.exists() and not (base / "rig9.del").exists(), "removed when stopped: rc=%s %r" % (r.returncode, r.stderr[-200:]))
print("FAIL" if fails else "PASS")
sys.exit(1 if fails else 0)
