"""make worktree-ready never seeds a binary as current over sources that differ from main's (WTREADY-STALE, 2026-10-27).

An agent found build/amiga/Amarchy dated 26 Oct without the packages.* index entries: the worktree's source was
older than main's binary (checkout before the binary was built) and differed from main's source, so make saw the
seeded binary as newer than its sources. No make runs: a temp main checkout and a temp git tree stand in, and the
mtimes are compared the way make does (target newer than source = up to date).
"""
import os
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import worktree_ready as wr  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


with tempfile.TemporaryDirectory() as d:
    d = pathlib.Path(d)
    main, tree = d / "main", d / "tree"
    (main / "build/amiga").mkdir(parents=True)
    tree.mkdir()
    subprocess.run(["git", "init", "-q", str(tree)], check=True)
    files = {"differs.c": (b"main version\n", b"tree version, more\n"), "same.c": (b"same\n", b"same\n"),
             "newonly.c": (None, b"only in tree\n")}
    for name, (m, t) in files.items():
        if m is not None:
            (main / name).write_bytes(m)
        (tree / name).write_bytes(t)
    subprocess.run(["git", "-C", str(tree), "add", "."], check=True)
    # tree checked out first (1000), main sources edited later (2000), main's binary built after that (3000)
    for name in files:
        os.utime(tree / name, (1000, 1000))
        if (main / name).exists():
            os.utime(main / name, (2000, 2000))
    (main / "build/amiga/Amarchy").write_bytes(b"bin")
    os.utime(main / "build/amiga/Amarchy", (3000, 3000))
    check(wr.seed(main / "build", tree / "build") >= 1, "binary seeded")
    wr.match_mtimes(main, tree)
    binm = (tree / "build/amiga/Amarchy").stat().st_mtime
    check(binm == 3000, "seeded binary keeps main's mtime")
    check((tree / "differs.c").stat().st_mtime > binm, "source differing from main is newer than the seeded binary")
    check((tree / "newonly.c").stat().st_mtime > binm, "source main lacks is newer than the seeded binary")
    check((tree / "same.c").stat().st_mtime <= binm, "source equal to main's stays older than the binary")

print("FAIL" if fails else "OK: worktree_ready")
sys.exit(1 if fails else 0)
