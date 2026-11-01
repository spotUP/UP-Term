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
    copy, ssh, _info = r.stdout.splitlines()
    assert copy.startswith("tar -czf - --no-xattrs --exclude home") and " | ssh " in copy and "--delete" not in copy
    assert "admin@nas.example" in copy and "-C /volume1/docker/claude-amiga" in copy and "-p 2222" in copy
    assert str(ROOT / "tools") + " uptelnetd.py" in copy
    assert "sudo docker compose build && sudo docker compose up -d" in ssh


def test_dry_run_does_not_claim_env_missing(tmp_path):
    r = run(tmp_path, "--dry-run")
    assert "No .env" not in r.stdout and "[NOTE]" not in r.stdout
    assert "[INFO] would check for .env in /volume1/docker/claude-amiga on the NAS" in r.stdout


def test_env_example_sent_but_not_env(tmp_path):
    pack = run(tmp_path, "--dry-run").stdout.splitlines()[0].split(" | ")[0].split()
    assert "env.example" in pack
    assert not any(a.endswith("/.env") or a == ".env" for a in pack)


def test_env_never_copied():
    assert ".env" not in d.copy_list()
    assert (ROOT / "tools/nas/env.example").is_file()


def test_compose_has_no_site_values():
    import re
    text = (ROOT / "tools/nas/compose.yaml").read_text()
    for key in ("NAS_IP", "ALLOW"):
        m = re.search(r"^\s*%s:\s*(.+?)\s*(#.*)?$" % key, text, re.M)
        assert m, key + " missing"
        v = m.group(1).strip().strip('"')
        assert re.fullmatch(r"\$\{%s:-EDIT-ME[^}]*\}" % key, v), "literal %s in compose.yaml: %s" % (key, v)


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
        assert (d.local_base(s) / s).exists()
    for extra in ("compose.yaml", "README.md", "skills"):
        assert extra in lst


def test_ssh_never_falls_back_to_password(tmp_path):
    r = run(tmp_path, "--dry-run")
    copy, ssh, _info = r.stdout.splitlines()
    assert "BatchMode=yes" in copy and "BatchMode=yes" in ssh


def test_failed_key_login_stops_with_key_hint(tmp_path, monkeypatch, capsys):
    monkeypatch.setenv("NAS_CONF", str(tmp_path / "nas.conf"))
    (tmp_path / "nas.conf").write_text(CONF)
    calls = []
    monkeypatch.setattr(d.subprocess, "call", lambda cmd: calls.append(cmd) or 255)
    assert d.main([]) == 1
    out = capsys.readouterr().out
    assert "key login" in out and "No .env" not in out
    assert len(calls) == 1, "the copy must not run after a failed key login"


def test_copy_is_not_rsync(tmp_path):
    """Synology refuses rsync over ssh unless its rsync service is on; the copy must be a plain ssh command."""
    copy = run(tmp_path, "--dry-run").stdout.splitlines()[0]
    assert "rsync" not in copy


def test_send_round_trip(tmp_path):
    """pack | unpack really carries the copy list: unpack into a local folder with sh instead of ssh."""
    (pack, _unpack), _ssh = d.commands({"NAS_HOST": "h", "NAS_USER": "u", "NAS_DIR": "/x", "NAS_PORT": "22",
                                         "NAS_DOCKER": "docker"})
    out = tmp_path / "nas"
    out.mkdir()
    assert d.send(pack, ["tar", "-xzf", "-", "-C", str(out)]) == 0
    for n in d.copy_list():
        assert (out / n).exists(), n
    assert not list(out.rglob("._*")) and not (out / ".env").exists()


def test_wasabi_clone_source_is_one_file():
    """The pinned wasabi commit can exist only on the fork: Dockerfile and Makefile clone from wasabi.repo, no literal URL."""
    df = (ROOT / "tools/nas/Dockerfile").read_text()
    clone = [l for l in df.splitlines() if "git clone" in l and "wasabi" in l]
    assert clone and all("/opt/wasabi/wasabi.repo" in l and "github.com" not in l for l in clone)
    assert "wasabi.repo" in d.copy_list()
    mk = (ROOT / "Makefile").read_text()
    assert "WASABI_REPO := $(strip $(shell cat tools/nas/wasabi.repo))" in mk
    assert (ROOT / "tools/nas/wasabi.repo").read_text().strip() == "https://github.com/spotUP/wasabi"
