"""mkkit.py refuses a kit whose Files/libs/ixnet.library opens AmiTCP:libs/usergroup.library (2026-10-28:
a kit install copied such an ixnet into SYS:Libs and every ixemul program asked for the volume AmiTCP:)."""
import pathlib, subprocess, sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import libguard, mkkit

OLD = b"\0ixnet\0AmiTCP:libs/usergroup.library\0"


def stage(tmp_path, monkeypatch, data):
    """A staged base kit (build/dist/up-term/Files/libs/ixnet.library = data) under a temp BUILD."""
    libs = tmp_path / 'dist/up-term/Files/libs'
    libs.mkdir(parents=True)
    (libs / 'ixnet.library').write_bytes(data)
    monkeypatch.setattr(mkkit, 'BUILD', tmp_path)


def test_mkkit_refuses_ixnet_that_opens_amitcp(tmp_path, monkeypatch):
    stage(tmp_path, monkeypatch, OLD)
    with pytest.raises(SystemExit) as e:
        mkkit.main(['base'])
    assert 'AmiTCP:libs/usergroup.library' in str(e.value)


def test_mkkit_accepts_clean_ixnet_past_the_guard(tmp_path, monkeypatch):
    stage(tmp_path, monkeypatch, b"\0ixnet\0")
    try:
        mkkit.main(['base'])
    except BaseException as e:  # later steps need the full build; only the guard's message matters
        assert 'AmiTCP' not in str(e)


def test_cli_exit_status(tmp_path):
    bad = tmp_path / 'ixnet.library'
    bad.write_bytes(OLD)
    r = subprocess.run([sys.executable, str(ROOT / 'tools/libguard.py'), str(bad)], capture_output=True, text=True)
    assert r.returncode == 1 and 'AmiTCP' in r.stderr
