"""tools/rig/paths.py: a rig run from a git worktree uses the main checkout's
build/rig<N>, not one under the worktree.

Regression (2026-10-12): `UPTERM_RIG=2 amigapi_rig.py` in a worktree started
FS-UAE on <worktree>/build/rig2/vtcon-rig.fs-uae, which did not exist, and the
Amiga booted with no bootable device.
"""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


def git(*args, cwd):
    subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True)


def rig_of(checkout, rig_n):
    """paths.RIG and RIG_HOME as the copy of paths.py in `checkout` computes them."""
    env = {k: v for k, v in os.environ.items() if not k.startswith(("UPTERM_", "GIT_"))}
    env["UPTERM_RIG"] = str(rig_n)
    out = subprocess.run(
        [sys.executable, "-I", "-c",
         "import sys; sys.path.insert(0, sys.argv[1]); import paths; print(paths.RIG); print(paths.LIB68040)",
         str(checkout / "tools/rig")],
        env=env, capture_output=True, text=True, check=True).stdout.split("\n")
    return pathlib.Path(out[0]), pathlib.Path(out[1])


with tempfile.TemporaryDirectory() as tmp:
    main = pathlib.Path(tmp).resolve() / "vtcon"
    (main / "tools/rig").mkdir(parents=True)
    shutil.copyfile(HERE / "rig/paths.py", main / "tools/rig/paths.py")
    git("init", "-q", cwd=main)
    git("add", "tools/rig/paths.py", cwd=main)
    git("-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "paths", cwd=main)
    wt = main / ".claude/worktrees/phase-x"
    git("worktree", "add", "-q", "-b", "phase-x", str(wt), cwd=main)

    rig, lib = rig_of(main, 2)
    check(rig == main / "build/rig2", "main checkout: rig 2 is %s, not %s" % (rig, main / "build/rig2"))
    rig, lib = rig_of(wt, 2)
    check(rig == main / "build/rig2", "worktree: rig 2 is %s, not the main checkout's %s" % (rig, main / "build/rig2"))
    rig, lib = rig_of(wt, 1)
    check(rig == main / "build/rig", "worktree: rig 1 is %s, not %s" % (rig, main / "build/rig"))
    check(lib == main / "build/rig-inputs/68040.library", "worktree: rig inputs at %s" % lib)

    # not a git checkout (a source tarball): the rig stays under the checkout
    plain = pathlib.Path(tmp).resolve() / "plain"
    shutil.copytree(main / "tools", plain / "tools", ignore=shutil.ignore_patterns(".git"))
    rig, lib = rig_of(plain, 1)
    check(rig == plain / "build/rig", "no git: rig 1 is %s, not %s" % (rig, plain / "build/rig"))

if fails:
    sys.exit("%d failed" % fails)
print("[OK] rig_paths: 5 checks, 0 failed")
