#!/usr/bin/env python3
"""Make a fresh git worktree rig-ready without a full rebuild (`make worktree-ready`).

A worktree has no build/, and every tracked file in it is newer than any binary, so make would rebuild all of it.
This seeds build/ from the main checkout (APFS clones, mtimes kept; files the worktree already has are left alone),
then gives every tracked file whose content equals the main checkout's the main file's mtime. make then rebuilds
only what this branch changed. Never symlinks: both trees write build/. Rigs, logs, archives and the kit output are
not copied (rigs live in the main checkout's build/ for every worktree: tools/rig/paths.py).
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import paths  # noqa: E402

SKIP_DIRS = {"dist", "dist-src", "ipk", "legacy", "research", "upkgui", "wasabi", "sessions", "out", "pkgs", "p7",
             "kit-a1200", "screens", "twemoji", "iterm2-venv"}
SKIP_SUFFIX = (".log", ".lha", ".readme", ".lst", ".dSYM", ".old", ".sock", ".txt")


def skip(entry):
    n = entry.name
    if n.startswith(("rig", "streams-", "gate-")) or n in SKIP_DIRS:
        return True
    return n.endswith(SKIP_SUFFIX)


def seed(src, dst):
    dst.mkdir(exist_ok=True)
    n = 0
    for e in sorted(os.scandir(src), key=lambda e: e.name):
        d = dst / e.name
        if skip(e) or d.exists():
            continue
        # -c clones on APFS (instant, no space); -p keeps the mtimes make decides by
        if subprocess.run(["cp", "-cpR", e.path, str(d)], capture_output=True).returncode:
            subprocess.run(["cp", "-pR", e.path, str(d)], check=True)
        n += 1
    return n


def match_mtimes(main, tree):
    """Invariant: a seeded binary is current only if every source it was built from equals main's.
    A tracked file whose content equals main's gets main's mtime (make keeps the seeded binary). A file that
    differs from main's, or that main lacks, is touched to now, so it is newer than any seeded binary and make
    rebuilds it: its checkout mtime can be older than main's binary (worktree made earlier, binary built since)."""
    out = subprocess.run(["git", "-C", str(tree), "ls-files", "-z"], capture_output=True, check=True).stdout
    n = 0
    for name in out.decode().split("\0"):
        a, b = tree / name, main / name
        if not name or not a.is_file():
            continue
        if not b.is_file() or a.stat().st_size != b.stat().st_size or a.read_bytes() != b.read_bytes():
            os.utime(a)
            continue
        sa, sb = a.stat(), b.stat()
        if sa.st_mtime_ns <= sb.st_mtime_ns:
            continue
        os.utime(a, ns=(sb.st_atime_ns, sb.st_mtime_ns))
        n += 1
    return n


def main():
    main_co = paths.MAIN_CHECKOUT
    if main_co.resolve() == ROOT.resolve():
        print("[INFO] this is the main checkout: nothing to seed")
        return 0
    if not (main_co / "build").is_dir():
        print("[WARN] %s has no build/: nothing to seed, make builds all" % main_co)
        return 0
    c = seed(main_co / "build", ROOT / "build")
    m = match_mtimes(main_co, ROOT)
    print("[OK] worktree ready: %d build entries seeded from %s, %d unchanged sources dated as there" % (c, main_co, m))
    return 0


if __name__ == "__main__":
    sys.exit(main())
