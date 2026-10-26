#!/usr/bin/env python3
"""a1200_drift.py -- which UP-Term programs on a real Amiga differ from this checkout's build (build/amiga).

   AMI_HOST=<ip> AMI_PORT=<port> AMI_TOKEN=<key> python3 -I tools/rig/a1200_drift.py [--build DIR]

One line per installed file: `same`, `DIFF` or `missing`, with the file's path on the Amiga. The paths come from
the `Copy` lines of dist/install.dos (what the kit installs where), not from a second list: a program the kit
copies to C:, L:, DEVS: or UP-Term:bin is checked at that place. md5 is read on the Amiga (UP-Term:bin/md5sum)
over ami.py. Exit status 1 when anything differs or is missing. The rig's start-time [WARN] (rig.build_drift)
checks the same build files against a rig's VTC: copies; this is the same check against an installed Amiga."""
import hashlib, pathlib, re, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
AMI = HERE / "ami.py"
CHUNK = 12           # files per md5sum call (command line length)
MD5 = "UP-Term:bin/md5sum"


def install_map(install_dos_text, built_names):
    """[(build file name, Amiga path)] from the Copy lines of install.dos whose sources are built programs.
    `Copy a b DIR` puts every source in DIR; a single source goes to the named file, unless the destination
    ends in ':' or '/' (a drawer). Copies of drawers (ALL) and everything not built are skipped."""
    built, out = set(built_names), []
    for line in install_dos_text.splitlines():
        words = line.split()
        if not words or words[0].lower() != "copy" or any(w.upper() == "ALL" for w in words):
            continue
        args = [w for w in words[1:] if w.upper() not in ("CLONE", "QUIET", ">NIL:")]
        if len(args) < 2:
            continue
        *srcs, dest = args
        if not all(s in built for s in srcs):
            continue
        for s in srcs:
            if len(srcs) > 1 or dest.endswith((":", "/")):
                path = dest + ("" if dest.endswith((":", "/")) else "/") + s
            else:
                path = dest
            if (s, path) not in out:
                out.append((s, path))
    return out


def md5_file(path):
    return hashlib.md5(pathlib.Path(path).read_bytes()).hexdigest()


def parse_md5sum(output):
    """{amiga path: md5} from md5sum's lines; a file md5sum could not open is absent."""
    got = {}
    for line in output.splitlines():
        m = re.match(r"([0-9a-f]{32})\s+(\S.*)$", line)
        if m:
            got[m.group(2).strip()] = m.group(1)
    return got


def amiga_md5s(paths):
    got = {}
    for i in range(0, len(paths), CHUNK):
        r = subprocess.run([sys.executable, "-I", str(AMI), "exec", MD5 + " " + " ".join(paths[i:i + CHUNK]), "120"],
                           capture_output=True, text=True)
        got.update(parse_md5sum(r.stdout))
    return got


def verdicts(entries, build, amiga):
    """[(state, build name, amiga path)]; state same / DIFF / missing (path match ignores case: AmigaDOS)."""
    low = {k.lower(): v for k, v in amiga.items()}
    return [("missing" if low.get(p.lower()) is None else "same" if low[p.lower()] == build[n] else "DIFF", n, p)
            for n, p in entries]


def main(argv):
    bdir = pathlib.Path(argv[argv.index("--build") + 1]) if "--build" in argv else ROOT / "build/amiga"
    names = [f.name for f in bdir.iterdir() if f.is_file()]
    entries = install_map((ROOT / "dist/install.dos").read_text(errors="replace"), names)
    build = {n: md5_file(bdir / n) for n, _ in entries}
    res = verdicts(entries, build, amiga_md5s([p for _, p in entries]))
    for state, n, p in res:
        print("%-8s %-28s %s" % (state, n, p))
    bad = sum(1 for s, _, _ in res if s != "same")
    print("%d file(s), %d differ or missing" % (len(res), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
