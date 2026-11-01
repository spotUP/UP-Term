#!/usr/bin/env python3
"""Deploy the Claude-on-NAS drawer to the owner's own NAS: tar piped over ssh, then docker compose build + up.

Config (git-ignored, outside the repo): ~/.config/upterm/nas.conf, lines KEY=value, '#' comments.
Keys: NAS_HOST (required), NAS_USER (required), NAS_DIR (required, e.g. /volume1/docker/claude-amiga),
NAS_PORT (default 22), NAS_DOCKER (default "sudo docker"; Synology needs sudo for docker),
NAS_SSH (default "ssh"; on the Amiga bebbossh, whose flags differ: -p, -T, no -o, no -t).
Environment variables of the same names win over the file; NAS_CONF names another file.
Only the copy list is sent (files are overwritten, nothing is deleted), and home/ (the volume) is excluded.
tar over ssh, not rsync: Synology refuses rsync over ssh ("Permission denied, please try again") unless the
DSM rsync service is enabled; a plain ssh command needs no NAS setting.
.env (site settings NAS_IP, ALLOW) is never sent; if the remote folder has none, a note says to create it from env.example.
The archive is packed in-process with tarfile (no tar program needed, so it runs on the Amiga too); the NAS unpacks it with tar.
Run: python3 -I tools/nas/deploy.py [--dry-run] [--no-build] [--stage DIR]
"""
import argparse, os, pathlib, re, shlex, shutil, subprocess, sys, tarfile, tempfile

HERE = pathlib.Path(__file__).resolve().parent
TOOLS = HERE.parent
KEYS = ("NAS_HOST", "NAS_PORT", "NAS_USER", "NAS_DIR", "NAS_DOCKER", "NAS_SSH")
REQUIRED = ("NAS_HOST", "NAS_USER", "NAS_DIR")
DEFAULTS = {"NAS_PORT": "22", "NAS_DOCKER": "sudo docker", "NAS_SSH": "ssh"}
EXTRA = ("Dockerfile", "compose.yaml", "README.md", "env.example")  # not COPY sources, but the build and the compose project need them
# Key login only: BatchMode stops ssh falling back to a password prompt, which the deploy cannot answer
# (the user types the sudo password there instead and gets "Permission denied"). sudo on the NAS still prompts over -t.
SSH_OPTS = ("-o", "BatchMode=yes")


def conf_path():
    return pathlib.Path(os.environ.get("NAS_CONF") or "~/.config/upterm/nas.conf").expanduser()


def copy_sources(dockerfile=None):
    """COPY sources of the Dockerfile, in order: the one place the copy list comes from."""
    out = []
    for line in (dockerfile or HERE / "Dockerfile").read_text().splitlines():
        m = re.match(r"\s*COPY\s+(?:--\S+\s+)*(.+)", line)
        if m:
            parts = m.group(1).split()
            out.extend(parts[:-1])
    return out


def local_base(name):
    """The folder a COPY source lives in: the drawer, or tools/ (uptelnetd.py)."""
    for base in (HERE, TOOLS):
        if (base / name).exists():
            return base
    raise SystemExit("[ERROR] Dockerfile COPY source not found in tools/nas or tools/: " + name)


def copy_list(dockerfile=None):
    names = []
    for n in copy_sources(dockerfile) + list(EXTRA):
        if n not in names:
            names.append(n)
    return names


def load_config(env=None):
    env = os.environ if env is None else env
    path = conf_path()
    cfg = dict(DEFAULTS)
    if path.is_file():
        for line in path.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                cfg[k.strip()] = v.strip().strip('"').strip("'")
    for k in KEYS:
        if env.get(k):
            cfg[k] = env[k]
    missing = [k for k in REQUIRED if not cfg.get(k)]
    if missing:
        raise SystemExit("[ERROR] NAS config incomplete: missing %s. Put them in %s (KEY=value lines; keys %s; "
                         "NAS_PORT defaults to 22, NAS_DOCKER to \"sudo docker\") or export them."
                         % (", ".join(missing), path, ", ".join(KEYS)))
    return cfg


def pack_list():
    """(base folder, name) of every copy-list entry: what pack_to packs and stage copies."""
    return [(str(local_base(n)), n) for n in copy_list()]


def ssh_cmd(cfg, remote, tty=False):
    """The ssh argv for one remote command: NAS_SSH, then its flags. bebbossh has no -o and no -t: a pseudo terminal
    is its default (-T turns it off), the port is -p, and key login uses ENVARC:ssh/id_ed25519."""
    prog = shlex.split(cfg["NAS_SSH"])
    target = "%s@%s" % (cfg["NAS_USER"], cfg["NAS_HOST"])
    if os.path.basename(prog[0].replace(":", "/")).lower().startswith("bebbossh"):
        return [*prog, "-p", cfg["NAS_PORT"], *([] if tty else ["-T"]), target, remote]
    return [*prog, *(["-t"] if tty else []), *SSH_OPTS, "-p", cfg["NAS_PORT"], target, remote]


def commands(cfg, build=True):
    pack = pack_list()
    nas_dir = shlex.quote(cfg["NAS_DIR"])
    unpack = ssh_cmd(cfg, "mkdir -p %s && tar -xzf - --no-same-owner -C %s" % (nas_dir, nas_dir))
    dk = cfg["NAS_DOCKER"]
    steps = ["cd %s" % shlex.quote(cfg["NAS_DIR"])]
    if build:
        steps.append("%s compose build" % dk)
    steps.append("%s compose up -d" % dk)
    return (pack, unpack), ssh_cmd(cfg, " && ".join(steps), tty=True)


def pack_filter(ti):
    """Nothing of the volume (home/), no macOS ._ files, no owner (the NAS unpacks with --no-same-owner)."""
    parts = ti.name.split("/")
    if "home" in parts or any(p.startswith("._") for p in parts):
        return None
    ti.uid = ti.gid = 0
    ti.uname = ti.gname = ""
    return ti


def pack_to(fileobj, pack):
    """Write the copy list as a tar.gz to fileobj (streamed: nothing needs to seek)."""
    with tarfile.open(fileobj=fileobj, mode="w|gz", format=tarfile.GNU_FORMAT) as tf:
        for base, name in pack:
            tf.add(os.path.join(base, name), arcname=name, filter=pack_filter)


def pack_text(pack, unpack):
    flat = " ".join("-C %s %s" % (b, n) for b, n in pack)
    return "python-tarfile czf - --exclude home %s | %s" % (flat, shlex.join(unpack))


def send(pack, unpack, spool=None):
    """pack | unpack; 0 only when both ends succeed. With spool (a file path, T: on the Amiga) the archive is
    written there first and fed to ssh by a redirect, for systems where a Popen stdin pipe does not work."""
    if spool:
        with open(spool, "wb") as f:
            pack_to(f, pack)
        try:
            with open(spool, "rb") as f:
                return subprocess.call(unpack, stdin=f)
        finally:
            os.remove(spool)
    ssh = subprocess.Popen(unpack, stdin=subprocess.PIPE)
    try:
        pack_to(ssh.stdin, pack)
    except BrokenPipeError:
        pass
    finally:
        try:
            ssh.stdin.close()
        except BrokenPipeError:
            pass
    return ssh.wait()


def stage(dest):
    """Copy the drawer (the copy list and deploy.py) into dest: what up-claude ships as its nas/ drawer."""
    dest = pathlib.Path(dest)
    for base, name in pack_list():
        src = pathlib.Path(base) / name
        if src.is_dir():
            shutil.copytree(src, dest / name, ignore=shutil.ignore_patterns("home", "._*", "__pycache__"), dirs_exist_ok=True)
        else:
            dest.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest / name)
    shutil.copy2(HERE / "deploy.py", dest / "deploy.py")


def env_note(cfg):
    return ("[NOTE] No .env in %s on the NAS: create it from env.example (cp env.example .env, then edit NAS_IP and ALLOW). "
            "The container refuses to start on the EDIT-ME defaults. The deploy never sends .env." % cfg["NAS_DIR"])


def key_note(cfg):
    return ("[ERROR] ssh key login to %s@%s port %s failed (the deploy never asks for the account password). "
            "Check: %s true. Fix: ssh-copy-id -p %s %s@%s (bebbossh: key in ENVARC:ssh/id_ed25519, public key into the NAS ~/.ssh/authorized_keys), and on Synology enable "
            "the user home service (Control Panel > User > Advanced) so ~/.ssh/authorized_keys is read."
            % (cfg["NAS_USER"], cfg["NAS_HOST"], cfg["NAS_PORT"], shlex.join(ssh_cmd(cfg, "true")),
               cfg["NAS_PORT"], cfg["NAS_USER"], cfg["NAS_HOST"]))


def remote_has_env(cfg):
    """True or False for .env in NAS_DIR; None when ssh itself failed (exit 255: no key login)."""
    p = subprocess.call(ssh_cmd(cfg, "test -f %s" % shlex.quote(cfg["NAS_DIR"] + "/.env")))
    return None if p == 255 else p == 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dry-run", action="store_true", help="print the copy and ssh commands, run nothing")
    ap.add_argument("--no-build", action="store_true", help="skip docker compose build (copy and up -d only)")
    ap.add_argument("--stage", metavar="DIR", help="copy the drawer and deploy.py into DIR (the up-claude build), run nothing else")
    ap.add_argument("--spool", metavar="FILE", help="write the archive to FILE (T:nas.tgz on the Amiga) and feed ssh by a redirect, not a pipe")
    a = ap.parse_args(argv)
    if a.stage:
        stage(a.stage)
        return 0
    cfg = load_config()
    (pack, unpack), ssh = commands(cfg, build=not a.no_build)
    if a.dry_run:
        print(pack_text(pack, unpack))
        print(shlex.join(ssh))
        print("[INFO] would check for .env in %s on the NAS (the deploy never sends .env)" % cfg["NAS_DIR"])
        return 0
    has_env = remote_has_env(cfg)
    if has_env is None:
        print(key_note(cfg))
        return 1
    if not has_env:
        print(env_note(cfg))
    print("[INFO] " + pack_text(pack, unpack), flush=True)
    if send(pack, unpack, a.spool) != 0:
        print("[ERROR] failed: copy to the NAS (tar over ssh)")
        return 1
    print("[INFO] " + shlex.join(ssh), flush=True)
    if subprocess.call(ssh) != 0:
        print("[ERROR] failed: docker compose on the NAS")
        return 1
    print("[OK] deployed to %s:%s" % (cfg["NAS_HOST"], cfg["NAS_DIR"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
