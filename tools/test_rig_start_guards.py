"""rig.py start guards. RIG-PORT-CLASH (2026-10-30): rig 2's emulator listened on rig 8's wasabi port; start must refuse
(an [ERROR] line, non-zero exit, nothing written) when another process listens on its rig's port.
RIG-OS32-NODOPUS: --no-dopus on a host-directory system (--os / --os32) strips the Directory Opus User-Startup blocks."""
import pathlib
import socket
import sys

import pytest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
saved = sys.argv
sys.argv = ["rig.py", "test"]
import rig  # noqa: E402
sys.argv = saved


def listener():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    s.listen(1)
    return s, s.getsockname()[1]


def test_refuse_port_clash_names_error_and_port():
    s, port = listener()
    with s, pytest.raises(SystemExit) as e:
        rig.refuse_port_clash(port)
    assert str(e.value).startswith("[ERROR]") and str(port) in str(e.value)


def test_free_port_passes():
    s, port = listener()
    s.close()
    rig.refuse_port_clash(port)


def test_start_refuses_before_writing_anything(monkeypatch):
    """wired: _start (the entry of `rig.py start`) refuses before the system disk is cloned."""
    s, port = listener()
    monkeypatch.setattr(rig.paths, "WASABI_PORT", port)
    monkeypatch.setattr(rig, "mine", lambda: [])
    monkeypatch.setattr(rig, "fresh_sys_hdf", lambda: pytest.fail("wrote the system disk"))
    with s, pytest.raises(SystemExit) as e:
        rig._start(0)
    assert "[ERROR]" in str(e.value)


def test_no_dopus_strips_blocks_from_os_tree(tmp_path):
    (tmp_path / "S").mkdir()
    us = tmp_path / "S/User-Startup"
    us.write_bytes(b"; keep\n;BEGIN DirectoryOpus 5\nAssign DOpus5: x\n;END DirectoryOpus 5\n"
                   b";BEGIN DOpus5\nPath DOpus5:C\n;END DOpus5\n;BEGIN UP-Term\nAssign VTC: y\n;END UP-Term\n")
    rig.os_tree_no_dopus(tmp_path)
    t = us.read_text()
    assert "DOpus5" not in t and "Opus" not in t and "; keep" in t and "VTC:" in t


def test_no_dopus_on_clean_tree_is_noop(tmp_path):
    rig.os_tree_no_dopus(tmp_path)
