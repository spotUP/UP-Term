"""rig.py merge_build: a worktree without build/amiga/<file> installs the main checkout's (handler, vsh, any
built file), one [WARN] line per file naming it; its own handler wins when it has one (DEV-SPEED, 2026-10-27). No emulator runs."""
import contextlib
import io
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
saved = sys.argv
sys.argv = ["rig.py", "test"]
import rig  # noqa: E402
sys.argv = saved

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
    print(("PASS " if cond else "FAIL ") + what)


with tempfile.TemporaryDirectory() as t:
    t = pathlib.Path(t)
    tree, main = t / "tree", t / "main"
    for d in (tree, main):
        (d / "build/amiga").mkdir(parents=True)
    (main / "build/amiga/vtcon-handler").write_bytes(b"main")
    (main / "build/amiga/vsh").write_bytes(b"mainvsh")
    (main / "build/amiga/Amarchy").write_bytes(b"mainam")
    (tree / "build/amiga/Amarchy").write_bytes(b"ownam")
    old_root, old_main = rig.ROOT, rig.paths.MAIN_CHECKOUT
    rig.ROOT, rig.paths.MAIN_CHECKOUT = tree, main
    try:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rig.merge_build()
            got = rig.built_handler()
        check(got.resolve() == (main / "build/amiga/vtcon-handler").resolve() and (rig.build_amiga() / "vsh").read_bytes() == b"mainvsh",
              "no own handler or vsh: the main checkout's")
        check((rig.build_amiga() / "Amarchy").read_bytes() == b"ownam", "a file the tree has built wins")
        check(out.getvalue().count("[WARN]") == 2 and "vsh" in out.getvalue(), "one WARN line per file taken from main")
        check("[WARN]" in out.getvalue() and str(main) in out.getvalue(), "the WARN names the source")
        (tree / "build/amiga/vtcon-handler").write_bytes(b"own")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            (tree / "build/amiga/vsh").write_bytes(b"ownvsh")
            rig.merge_build()
            got = rig.built_handler()
        check(got.resolve() == (tree / "build/amiga/vtcon-handler").resolve() and "vtcon-handler" not in out.getvalue() and "vsh" not in out.getvalue(), "own files: used, only the rest warned")
    finally:
        rig.ROOT, rig.paths.MAIN_CHECKOUT = old_root, old_main
sys.exit(1 if fails else 0)
