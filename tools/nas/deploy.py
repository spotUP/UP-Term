#!/usr/bin/env python3
"""Deploy the Claude-on-NAS drawer to the owner's own NAS: tar piped over ssh, then docker compose build + up.

Config (git-ignored, outside the repo): ~/.config/upterm/nas.conf, lines KEY=value, '#' comments.
Keys: NAS_HOST (required), NAS_USER (required), NAS_DIR (required, e.g. /volume1/docker/claude-amiga),
NAS_PORT (default 22), NAS_DOCKER (default "sudo docker"; Synology needs sudo for docker).
Environment variables of the same names win over the file; NAS_CONF names another file.
Only the copy list is sent (files are overwritten, nothing is deleted), and home/ (the volume) is excluded.
tar over ssh, not rsync: Synology refuses rsync over ssh ("Permission denied, please try again") unless the
DSM rsync service is enabled; a plain ssh command needs no NAS setting.
.env (site settings NAS_IP, ALLOW) is never sent; if the remote folder has none, a note says to create it from env.example.
Run: python3 -I tools/nas/deploy.py [--dry-run] [--no-build]
"""
import argparse, os, pathlib, re, shlex, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
TOOLS = HERE.parent
KEYS = ("NAS_HOST", "NAS_PORT", "NAS_USER", "NAS_DIR", "NAS_DOCKER")
REQUIRED = ("NAS_HOST", "NAS_USER", "NAS_DIR")
DEFAULTS = {"NAS_PORT": "22", "NAS_DOCKER": "sudo docker"}
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


def commands(cfg, build=True):
    target = "%s@%s" % (cfg["NAS_USER"], cfg["NAS_HOST"])
    pack = ["tar", "-czf", "-", "--no-xattrs", "--exclude", "home"]  # no xattrs: GNU tar on the NAS warns on each macOS one
    for n in copy_list():
        pack += ["-C", str(local_base(n)), n]
    nas_dir = shlex.quote(cfg["NAS_DIR"])
    unpack = ["ssh", *SSH_OPTS, "-p", cfg["NAS_PORT"], target,
              "mkdir -p %s && tar -xzf - --no-same-owner -C %s" % (nas_dir, nas_dir)]
    dk = cfg["NAS_DOCKER"]
    steps = ["cd %s" % shlex.quote(cfg["NAS_DIR"])]
    if build:
        steps.append("%s compose build" % dk)
    steps.append("%s compose up -d" % dk)
    remote = " && ".join(steps)
    return (pack, unpack), ["ssh", "-t", *SSH_OPTS, "-p", cfg["NAS_PORT"], target, remote]


def send(pack, unpack):
    """Run pack | unpack; 0 only when both ends succeed. COPYFILE_DISABLE keeps macOS ._ files out of the archive."""
    tar = subprocess.Popen(pack, stdout=subprocess.PIPE, env=dict(os.environ, COPYFILE_DISABLE="1"))
    rc = subprocess.call(unpack, stdin=tar.stdout)
    tar.stdout.close()
    return tar.wait() or rc


def env_note(cfg):
    return ("[NOTE] No .env in %s on the NAS: create it from env.example (cp env.example .env, then edit NAS_IP and ALLOW). "
            "The container refuses to start on the EDIT-ME defaults. The deploy never sends .env." % cfg["NAS_DIR"])


def key_note(cfg):
    return ("[ERROR] ssh key login to %s@%s port %s failed (the deploy never asks for the account password). "
            "Check: ssh -o BatchMode=yes -p %s %s@%s true. Fix: ssh-copy-id -p %s %s@%s, and on Synology enable "
            "the user home service (Control Panel > User > Advanced) so ~/.ssh/authorized_keys is read."
            % ((cfg["NAS_USER"], cfg["NAS_HOST"], cfg["NAS_PORT"]) + (cfg["NAS_PORT"], cfg["NAS_USER"], cfg["NAS_HOST"]) * 2))


def remote_has_env(cfg):
    """True or False for .env in NAS_DIR; None when ssh itself failed (exit 255: no key login)."""
    p = subprocess.call(["ssh", *SSH_OPTS, "-p", cfg["NAS_PORT"], "%s@%s" % (cfg["NAS_USER"], cfg["NAS_HOST"]),
                         "test -f %s" % shlex.quote(cfg["NAS_DIR"] + "/.env")])
    return None if p == 255 else p == 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dry-run", action="store_true", help="print the copy and ssh commands, run nothing")
    ap.add_argument("--no-build", action="store_true", help="skip docker compose build (copy and up -d only)")
    a = ap.parse_args(argv)
    cfg = load_config()
    (pack, unpack), ssh = commands(cfg, build=not a.no_build)
    if a.dry_run:
        print(shlex.join(pack) + " | " + shlex.join(unpack))
        print(shlex.join(ssh))
        print("[INFO] would check for .env in %s on the NAS (the deploy never sends .env)" % cfg["NAS_DIR"])
        return 0
    has_env = remote_has_env(cfg)
    if has_env is None:
        print(key_note(cfg))
        return 1
    if not has_env:
        print(env_note(cfg))
    print("[INFO] " + shlex.join(pack) + " | " + shlex.join(unpack), flush=True)
    if send(pack, unpack) != 0:
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
