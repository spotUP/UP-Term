"""nas deploy: the copy list comes from the Dockerfile COPY lines; dry-run prints rsync (home excluded) and the ssh docker command."""
import pathlib, subprocess, sys
import pytest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/nas"))
import deploy as d

CONF = "NAS_HOST=nas.example\nNAS_USER=admin\nNAS_DIR=/volume1/docker/claude-amiga\nNAS_PORT=2222\n"


def run(tmp_path, *args, conf=CONF):
    f = tmp_path / "nas.conf"
    if conf is not None:
        f.write_text(conf)
    env = {"PATH": "/usr/bin:/bin", "NAS_CONF": str(f)}
    return subprocess.run([sys.executable, "-I", str(ROOT / "tools/nas/deploy.py"), *args],
                          capture_output=True, text=True, env=env)


def test_dry_run_prints_rsync_and_ssh(tmp_path):
    r = run(tmp_path, "--dry-run")
    assert r.returncode == 0, r.stderr
    rsync, ssh = r.stdout.splitlines()
    assert rsync.startswith("rsync ") and "--exclude home" in rsync and "--delete" not in rsync
    assert "admin@nas.example:/volume1/docker/claude-amiga/" in rsync and "ssh -p 2222" in rsync
    assert "tools/uptelnetd.py" in rsync
    assert "sudo docker compose build && sudo docker compose up -d" in ssh


def test_no_build(tmp_path):
    r = run(tmp_path, "--dry-run", "--no-build")
    assert "compose build" not in r.stdout and "compose up -d" in r.stdout


def test_missing_config_names_file_and_keys(tmp_path):
    r = run(tmp_path, "--dry-run", conf=None)
    assert r.returncode != 0
    assert str(tmp_path / "nas.conf") in r.stderr and "NAS_HOST" in r.stderr and "NAS_DIR" in r.stderr


def test_copy_list_covers_every_dockerfile_copy_source():
    srcs = [l.split()[1] for l in (ROOT / "tools/nas/Dockerfile").read_text().splitlines() if l.startswith("COPY ")]
    assert "settings.json" in srcs and "relay-config" in srcs and "uptelnetd.py" in srcs
    lst = d.copy_list()
    for s in srcs:
        assert s in lst
        assert d.local_path(s).exists()
    for extra in ("compose.yaml", "README.md", "skills"):
        assert extra in lst
