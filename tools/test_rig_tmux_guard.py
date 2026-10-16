"""tools/rig/tmux_rig.py check_tmux_current: the rig refuses a tmux-bin without
its $STACK cookie (regression, FP0.2 2026-10-15: a stale build ran on vsh's
16000-byte stack and failed). No rig boot: fake binaries in a temp tree beside a
copy of tmux-amiga's real tests/test_stack_cookie.py."""
import pathlib
import shutil
import sys

import pytest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import tmux_rig  # noqa: E402

REAL_TEST = tmux_rig.TMUX.parent.parent / "tests/test_stack_cookie.py"


def tree(tmp_path, binary):
    (tmp_path / "tests").mkdir()
    (tmp_path / "build").mkdir()
    shutil.copyfile(REAL_TEST, tmp_path / "tests/test_stack_cookie.py")
    (tmp_path / "build/tmux-bin").write_bytes(binary)
    return tmp_path / "build/tmux-bin"


def test_guard_aborts_without_cookie(tmp_path):
    with pytest.raises(SystemExit) as e:
        tmux_rig.check_tmux_current(tree(tmp_path, b"\0tmux no cookie\0"))
    assert "rebuild tmux-amiga: make -f Makefile.amiga" in str(e.value)


def test_guard_aborts_on_small_cookie(tmp_path):
    with pytest.raises(SystemExit):
        tmux_rig.check_tmux_current(tree(tmp_path, b"\0$STACK: 16000\0"))


def test_guard_passes_with_cookie(tmp_path):
    tmux_rig.check_tmux_current(tree(tmp_path, b"\0$STACK: 262144\0"))
