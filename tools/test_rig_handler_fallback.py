"""rig.py built_handler: a worktree without build/amiga/vtcon-handler installs the main checkout's, with a [WARN]
naming it; its own handler wins when it has one (DEV-SPEED, 2026-10-27). No emulator runs."""
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
    old_root, old_main = rig.ROOT, rig.paths.MAIN_CHECKOUT
    rig.ROOT, rig.paths.MAIN_CHECKOUT = tree, main
    try:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            got = rig.built_handler()
        check(got == main / "build/amiga/vtcon-handler", "no own handler: the main checkout's")
        check("[WARN]" in out.getvalue() and str(main) in out.getvalue(), "the WARN names the source")
        (tree / "build/amiga/vtcon-handler").write_bytes(b"own")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            got = rig.built_handler()
        check(got == tree / "build/amiga/vtcon-handler" and not out.getvalue(), "own handler: used, silent")
    finally:
        rig.ROOT, rig.paths.MAIN_CHECKOUT = old_root, old_main
sys.exit(1 if fails else 0)
