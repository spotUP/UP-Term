"""nas deploy: the copy list comes from the Dockerfile COPY lines; dry-run prints rsync (home excluded) and the ssh docker command."""
import pathlib, shlex, subprocess, sys
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
    assert copy.startswith("python-tarfile czf - --exclude home") and " | ssh " in copy and "--delete" not in copy
    assert "admin@nas.example" in copy and "-C /volume1/docker/claude-amiga" in copy and "-p 2222" in copy
    assert str(ROOT / "tools") + " uptelnetd.py" in copy
    assert 'sudo "$(command -v docker)" compose build && sudo "$(command -v docker)" compose up -d' in ssh


def test_default_docker_is_found_in_usr_local_bin(tmp_path):
    """Synology: a non-login ssh PATH is /usr/bin:/bin:/usr/sbin:/sbin, docker is /usr/local/bin/docker, so a bare
    'sudo docker' is 'command not found'. The remote command adds /usr/local/bin and resolves docker before sudo."""
    r = run(tmp_path, "--dry-run")
    ssh = r.stdout.splitlines()[1]
    remote = shlex.split(ssh)[-1]
    assert remote.startswith('export PATH="$PATH:/usr/local/bin" && cd ')
    fake = tmp_path / "usrlocal"
    fake.mkdir()
    (fake / "docker").write_text("#!/bin/sh\necho DOCKER \"$@\"\n")
    (fake / "docker").chmod(0o755)
    bin_ = tmp_path / "bin"  # a sudo that runs its command, ahead of the real one; docker stays only in "/usr/local/bin"
    bin_.mkdir()
    (bin_ / "sudo").write_text('#!/bin/sh\nexec "$@"\n')
    (bin_ / "sudo").chmod(0o755)
    nas_dir = tmp_path / "nasdir"
    nas_dir.mkdir()
    script = remote.replace("/usr/local/bin", str(fake)).replace("/volume1/docker/claude-amiga", str(nas_dir))
    out = subprocess.run(["/bin/sh", "-c", script], capture_output=True, text=True,
                         env={"PATH": "%s:/usr/bin:/bin:/usr/sbin:/sbin" % bin_})
    assert out.returncode == 0, out.stderr
    assert out.stdout.splitlines() == ["DOCKER compose build", "DOCKER compose up -d"]


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
    (pack, _unpack), _ssh = d.commands(CFG)
    out = tmp_path / "nas"
    out.mkdir()
    assert d.send(pack, ["tar", "-xzf", "-", "-C", str(out)]) == 0
    for n in d.copy_list():
        assert (out / n).exists(), n
    assert not list(out.rglob("._*")) and not (out / ".env").exists()


def test_wasabi_clone_source_is_one_file():
    """The clone source must hold the pinned wasabi commit: Dockerfile and Makefile clone from wasabi.repo, no literal URL."""
    df = (ROOT / "tools/nas/Dockerfile").read_text()
    clone = [l for l in df.splitlines() if "git clone" in l and "wasabi" in l]
    assert clone and all("/opt/wasabi/wasabi.repo" in l and "github.com" not in l for l in clone)
    assert "wasabi.repo" in d.copy_list()
    mk = (ROOT / "Makefile").read_text()
    assert "WASABI_REPO := $(strip $(shell cat tools/nas/wasabi.repo))" in mk
    assert (ROOT / "tools/nas/wasabi.repo").read_text().strip() == "https://github.com/creep-ltx/wasabi"


CFG = {"NAS_HOST": "h", "NAS_USER": "u", "NAS_DIR": "/x", "NAS_PORT": "22", "NAS_DOCKER": "docker", "NAS_SSH": "ssh"}


def test_tarfile_pack_members_equal_copy_list(tmp_path):
    """The in-process pack carries exactly the copy list: top-level members == copy_list(), no home/, no ._ files."""
    import io, tarfile
    home = d.HERE / "home"
    made = not home.exists()
    home.mkdir(exist_ok=True)
    (home / "keep-out").write_text("x")
    try:
        buf = io.BytesIO()
        d.pack_to(buf, d.pack_list())
    finally:
        (home / "keep-out").unlink()
        if made:
            home.rmdir()
    buf.seek(0)
    names = tarfile.open(fileobj=buf, mode="r:gz").getnames()
    assert sorted({n.split("/")[0] for n in names}) == sorted(d.copy_list())
    assert not any("home" in n.split("/") or n.split("/")[-1].startswith("._") for n in names)


def test_pack_filter_drops_home_and_appledouble_and_owner():
    import tarfile
    for n in ("home", "skills/home/x", "skills/._x", "._Dockerfile"):
        assert d.pack_filter(tarfile.TarInfo(n)) is None, n
    ti = tarfile.TarInfo("skills/a")
    ti.uid = ti.gid = 501
    ti.uname = "mac"
    out = d.pack_filter(ti)
    assert (out.uid, out.gid, out.uname) == (0, 0, "")


def test_send_does_not_spawn_a_tar_program(monkeypatch, tmp_path):
    """One code path for Mac and Amiga: the pack is Python tarfile, the only subprocess is the unpack end."""
    calls = []
    real = subprocess.Popen
    monkeypatch.setattr(d.subprocess, "Popen", lambda cmd, **kw: calls.append(cmd) or real(cmd, **kw))
    out = tmp_path / "o"
    out.mkdir()
    assert d.send(d.pack_list(), ["tar", "-xzf", "-", "-C", str(out)]) == 0
    assert calls == [["tar", "-xzf", "-", "-C", str(out)]]
    assert (out / "Dockerfile").is_file()


def test_send_spool_feeds_ssh_by_redirect(tmp_path):
    out = tmp_path / "o"
    out.mkdir()
    spool = tmp_path / "T-nas.tgz"
    assert d.send(d.pack_list(), ["tar", "-xzf", "-", "-C", str(out)], spool=str(spool)) == 0
    assert (out / "Dockerfile").is_file() and not spool.exists()


def test_nas_ssh_reaches_every_ssh_argv(tmp_path, monkeypatch):
    """NAS_SSH is the program of the unpack, the docker step and the .env check (and key_note), default ssh."""
    cfg = dict(CFG, NAS_SSH="/opt/mine/myssh")
    (_pack, unpack), docker = d.commands(cfg)
    assert unpack[0] == docker[0] == "/opt/mine/myssh"
    seen = []
    monkeypatch.setattr(d.subprocess, "call", lambda cmd, **kw: seen.append(cmd) or 0)
    d.remote_has_env(cfg)
    assert seen[0][0] == "/opt/mine/myssh"
    assert "/opt/mine/myssh" in d.key_note(cfg)
    assert d.commands(CFG)[0][1][0] == "ssh"
    assert d.load_config({})["NAS_SSH"] == "ssh"
    assert d.load_config({"NAS_SSH": "bebbossh"})["NAS_SSH"] == "bebbossh"


def test_bebbossh_flags():
    """bebbossh: -p port, -T when no terminal, no -o and no -t (it has neither); the docker step keeps its terminal."""
    cfg = dict(CFG, NAS_SSH="C:bebbossh", NAS_PORT="1337")
    (_pack, unpack), docker = d.commands(cfg)
    assert unpack[:4] == ["C:bebbossh", "-p", "1337", "-T"] and "-o" not in unpack and "-t" not in unpack
    assert docker[:3] == ["C:bebbossh", "-p", "1337"] and "-T" not in docker and "-t" not in docker and "-o" not in docker


def test_stage_ships_the_copy_list_and_deploy_py(tmp_path):
    d.stage(tmp_path / "nas")
    for n in d.copy_list() + ["deploy.py"]:
        assert (tmp_path / "nas" / n).exists(), n
    assert not (tmp_path / "nas" / "home").exists()
    r = subprocess.run([sys.executable, "-I", str(tmp_path / "nas/deploy.py"), "--dry-run"], capture_output=True, text=True,
                       env={"PATH": "/usr/bin:/bin", "NAS_CONF": str(tmp_path / "c")} | {"NAS_HOST": "h", "NAS_USER": "u", "NAS_DIR": "/x"})
    assert r.returncode == 0, r.stderr
    assert str(tmp_path / "nas") + " uptelnetd.py" in r.stdout


def test_without_zlib_the_archive_is_a_plain_tar(monkeypatch, tmp_path):
    """The Amiga Python has no zlib: pack a plain tar and unpack it with tar -xf (no z), not a gzip the Amiga cannot write."""
    import io, tarfile
    monkeypatch.setattr(d, "have_gzip", lambda: False)
    (pack, unpack), _ = d.commands(CFG)
    assert unpack[-1].count("tar -xf - --no-same-owner") == 1 and "-xzf" not in unpack[-1]
    buf = io.BytesIO()
    d.pack_to(buf, pack)
    buf.seek(0)
    assert buf.getvalue()[:2] != b"\x1f\x8b"
    assert "Dockerfile" in tarfile.open(fileobj=buf, mode="r:").getnames()
    monkeypatch.setattr(d, "have_gzip", lambda: True)
    assert "tar -xzf - " in d.commands(CFG)[0][1][-1]


def test_amiga_spools_by_default(monkeypatch):
    monkeypatch.setattr(d.sys, "platform", "amigaos")
    assert d.default_spool() == "T:nas.tar"
    monkeypatch.setattr(d.sys, "platform", "darwin")
    assert d.default_spool() is None
