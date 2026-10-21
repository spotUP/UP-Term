"""rig.py status: fs-uae alive but wasabid silent, with a CPU exception and CPU resets in the fs-uae log,
says "crashed" (exception line, reset count), not plain "down" (2026-10-22: a Guru on rig 4 became a reset
loop and read as "wasabid died"). Fixture: the relevant lines of the preserved crash log. Also: userland_rig.py
takes paths.rig_lock (a second holder is refused)."""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import paths  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


d = pathlib.Path(tempfile.mkdtemp())
check(paths.fsuae_crash(d) is None, "no log: no crash")
shutil.copy(HERE / "fixtures/fsuae-guru-reset-loop.log", d / "fs-uae.log.txt")
c = paths.fsuae_crash(d)
check(c is not None and "Illegal instruction: 0fff at 000108D2" in c[0] and c[1] == 2, "fixture reports exception and 2 resets: %r" % (c,))
(d / "fs-uae.log.txt").write_text("CPU reset PC=f80dc8 (Kickstart ROM)..\n")
check(paths.fsuae_crash(d) is None, "a reset with no exception before it (normal reboot) is not a crash")

# rig.py status end to end: fake mine() and ami.req
code = ("import sys,pathlib; sys.path.insert(0,%r); sys.argv=['rig.py']; import rig, ami, paths\n"
        "paths.FSUAE_LOG_DIR=pathlib.Path(%r)\n"
        "def boom(*a,**k): raise OSError('no answer')\n"
        "ami.req=boom\nrig.mine=lambda: ['fs-uae']\nrig.status()\n" % (str(HERE / "rig"), str(d)))
shutil.copy(HERE / "fixtures/fsuae-guru-reset-loop.log", d / "fs-uae.log.txt")
r = subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True)
check(r.stdout.startswith("crashed: Illegal instruction") and "2 CPU reset" in r.stdout, "status says crashed: %r %s" % (r.stdout, r.stderr[-300:]))
(d / "fs-uae.log.txt").write_text("")
r = subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True)
check(r.stdout.strip() == "down", "clean log: down: %r" % r.stdout)

# userland_rig takes the rig lock
rd = pathlib.Path(tempfile.mkdtemp())
holder = subprocess.Popen([sys.executable, "-I", "-c",
                           "import sys,pathlib,time; sys.path.insert(0,%r); import paths\n"
                           "with paths.rig_lock(pathlib.Path(%r)):\n print('held',flush=True); time.sleep(30)\n"
                           % (str(HERE / "rig"), str(rd))], stdout=subprocess.PIPE, text=True)
try:
    holder.stdout.readline()
    env = dict(os.environ, UPTERM_RIG_DIR=str(rd))
    code2 = ("import sys,pathlib; sys.path.insert(0,%r); sys.argv=['userland_rig.py','--only','none']; import paths\n"
             "import contextlib\n_orig=paths.rig_lock\npaths.rig_lock=lambda d=None:_orig(pathlib.Path(%r))\n"
             "import userland_rig; userland_rig.kit_assigns=lambda: print('RAN')\nsys.exit(userland_rig.main())\n"
             % (str(HERE / "rig"), str(rd)))
    r = subprocess.run([sys.executable, "-I", "-c", code2], capture_output=True, text=True, env=env, timeout=60)
    check("RAN" not in r.stdout and r.returncode != 0, "userland_rig refused while locked: rc=%s %r %s" % (r.returncode, r.stdout[-100:], r.stderr[-200:]))
finally:
    holder.kill()
print("FAIL: %d" % fails if fails else "rig crashed-status + userland lock: ok")
sys.exit(1 if fails else 0)
