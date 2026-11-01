#!/usr/bin/env python3
"""Deploy the Claude-on-NAS drawer to the owner's own NAS: rsync over ssh, then docker compose build + up.

Config (git-ignored, outside the repo): ~/.config/upterm/nas.conf, lines KEY=value, '#' comments.
Keys: NAS_HOST (required), NAS_USER (required), NAS_DIR (required, e.g. /volume1/docker/claude-amiga),
NAS_PORT (default 22), NAS_DOCKER (default "sudo docker"; Synology needs sudo for docker).
Environment variables of the same names win over the file; NAS_CONF names another file.
Only the copy list is sent, never with --delete, and home/ (the volume) is excluded.
Run: python3 -I tools/nas/deploy.py [--dry-run] [--no-build]
"""
import argparse, os, pathlib, re, shlex, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
TOOLS = HERE.parent
KEYS = ("NAS_HOST", "NAS_PORT", "NAS_USER", "NAS_DIR", "NAS_DOCKER")
REQUIRED = ("NAS_HOST", "NAS_USER", "NAS_DIR")
DEFAULTS = {"NAS_PORT": "22", "NAS_DOCKER": "sudo docker"}
EXTRA = ("Dockerfile", "compose.yaml", "README.md")  # not COPY sources, but the build and the compose project need them


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


def local_path(name):
    """A COPY source lives in the drawer, or in tools/ (uptelnetd.py)."""
    for base in (HERE, TOOLS):
        if (base / name).exists():
            return base / name
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
    ssh = "ssh -p %s" % cfg["NAS_PORT"]
    target = "%s@%s" % (cfg["NAS_USER"], cfg["NAS_HOST"])
    srcs = [str(local_path(n)) for n in copy_list()]
    rsync = ["rsync", "-az", "--exclude", "home", "-e", ssh] + srcs + ["%s:%s/" % (target, cfg["NAS_DIR"])]
    dk = cfg["NAS_DOCKER"]
    steps = ["cd %s" % shlex.quote(cfg["NAS_DIR"])]
    if build:
        steps.append("%s compose build" % dk)
    steps.append("%s compose up -d" % dk)
    remote = " && ".join(steps)
    return rsync, ["ssh", "-t", "-p", cfg["NAS_PORT"], target, remote]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dry-run", action="store_true", help="print the rsync and ssh commands, run nothing")
    ap.add_argument("--no-build", action="store_true", help="skip docker compose build (copy and up -d only)")
    a = ap.parse_args(argv)
    cfg = load_config()
    rsync, ssh = commands(cfg, build=not a.no_build)
    if a.dry_run:
        print(shlex.join(rsync))
        print(shlex.join(ssh))
        return 0
    for cmd in (rsync, ssh):
        print("[INFO] " + shlex.join(cmd), flush=True)
        if subprocess.call(cmd) != 0:
            print("[ERROR] failed: " + cmd[0])
            return 1
    print("[OK] deployed to %s:%s" % (cfg["NAS_HOST"], cfg["NAS_DIR"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
