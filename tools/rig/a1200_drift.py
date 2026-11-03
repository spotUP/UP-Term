#!/usr/bin/env python3
"""a1200_drift.py -- which UP-Term programs on a real Amiga differ from this checkout's build (build/amiga).

   AMI_HOST=<ip> AMI_PORT=<port> AMI_TOKEN=<key> python3 -I tools/rig/a1200_drift.py [--build DIR] [--nofpu]

One line per installed file: `same`, `DIFF` or `missing`, with the file's path on the Amiga. The paths come from
the `Copy` lines of dist/install.dos (what the kit installs where), not from a second list: a program the kit
copies to C:, L:, DEVS: or UP-Term:bin is checked at that place; dist/keymap is checked at ENVARC:up-term/keymap
and ENV:up-term/keymap (DIST_SOURCES). The kit's patched libraries, which the Installer
'libs' step puts in LIBS:, are not Copy lines of a built program: LIBS:ixemul.library and LIBS:ixnet.library are
checked against paths.IXEMUL_LIB (paths.IXEMUL_LIB_SOFT with --nofpu, the build for a machine with no FPU) and
paths.IXNET_LIB, the files rig.py start stages in VTC:ixp6. md5 is read on the Amiga (UP-Term:bin/md5sum)
over ami.py. Exit status 1 when anything differs or is missing. The rig's start-time [WARN] (rig.build_drift)
checks the same build files against a rig's VTC: copies; this is the same check against an installed Amiga."""
import hashlib, os, pathlib, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
AMI = HERE / "ami.py"
CHUNK = 12           # files per md5sum call (command line length)
MD5 = "UP-Term:bin/md5sum"
# Files the kit copies from dist/ rather than build/amiga, checked like built programs. keymap is checked whole:
# the user's own rows live in ENVARC:up-term/keymap.user, so the installed keymap must equal dist/keymap exactly.
# solidwin/SolidWin is Thomas Rapp's binary the kit ships unchanged (Copy solidwin/SolidWin C:SolidWin); listing it here
# puts C:SolidWin in every rig's kit set (kitset.derive, RIG-SOLIDWIN) and in the A1200 drift check.
DIST_SOURCES = ("keymap", "solidwin/SolidWin")
# Data the kit lays down, by install.dos source name -> file in dist/. lucide.dat and outline.dat are built into dist/Amarchy
# (make lucide-dat); bar.conf is the user's once it exists (install.dos copies it only when absent), so it is
# checked for existence only (USER_OWNED); the theme drawer (`Copy amarchy-themes <dir> ALL`) is DATA_DRAWERS.
DATA_FILES = {"lucide/lucide.dat": "Amarchy/Icons/lucide.dat", "lucide/outline.dat": "Amarchy/Icons/outline.dat", "bar.conf": "bar.conf"}
DATA_DRAWERS = {"amarchy-themes": "amarchy-themes"}
USER_OWNED = ("bar.conf",)
# Media the kit lays in the UP-Term: drawer (install.dos `Copy updemo/earth.anim UP-Term:share/updemo`), by path in the
# drawer -> file in dist/. A rig whose UP-Term: tree predates one of these shows UPDemo's coded fallback, not the media
# (RIG-DRIFT-MEDIA, rig 91, 2026-11-03: no earth.anim, the kitty scene judged on the wrong picture).
KIT_MEDIA = {"share/updemo/earth.anim": "updemo/earth.anim"}


def lib_entries(nofpu=False):
    """[(name, Amiga path, source file)] for the kit's ixemul.library and ixnet.library, sources from paths.py.
    LIBS: may be several drawers; md5sum opens LIBS:<name> the way every program does, so it reads the first one,
    the copy that is in use (install.dos replaces the one Which finds, which is that same first one)."""
    ixemul = paths.IXEMUL_LIB_SOFT if nofpu else paths.IXEMUL_LIB
    return [("ixemul.library", "LIBS:ixemul.library", ixemul), ("ixnet.library", "LIBS:ixnet.library", paths.IXNET_LIB)]


def install_map(install_dos_text, built_names):
    """[(build file name, Amiga path)] from the Copy lines of install.dos whose sources are built programs.
    `Copy a b DIR` puts every source in DIR; a single source goes to the named file, unless the destination
    ends in ':' or '/' (a drawer). Copies of drawers (ALL) and sources not built are skipped; a
    line's other, built sources are still checked."""
    built, out = set(built_names), []
    for line in install_dos_text.splitlines():
        words = line.split()
        if not words or words[0].lower() != "copy" or any(w.upper() == "ALL" for w in words):
            continue
        args = [w for w in words[1:] if w.upper() not in ("CLONE", "QUIET", ">NIL:")]
        if len(args) < 2:
            continue
        *srcs, dest = args
        for s in (s for s in srcs if s in built):
            if len(srcs) > 1 or dest.endswith((":", "/")):
                path = dest + ("" if dest.endswith((":", "/")) else "/") + s
            else:
                path = dest
            if (s, path) not in out:
                out.append((s, path))
    return out


def data_entries(install_dos_text, dist=None):
    """[(name, Amiga path, source file)] for the data install.dos copies: single files in DATA_FILES, and every
    file of each DATA_DRAWERS drawer (`Copy <drawer> <dest> ALL`) at <dest>/<file>. Paths come from the Copy lines."""
    dist = pathlib.Path(dist) if dist else ROOT / "dist"
    base = lambda n: n.rsplit("/", 1)[-1]
    # `Copy lucide/lucide.dat UP-Term:Amarchy/Icons`: a dest not named like the source is a drawer
    out = [(n, p if p.lower().endswith(("/" + base(n).lower(), ":" + base(n).lower())) else p + "/" + base(n), dist / DATA_FILES[n])
           for n, p in install_map(install_dos_text, list(DATA_FILES))]
    for line in install_dos_text.splitlines():
        words = line.split()
        if len(words) > 3 and words[0].lower() == "copy" and words[1] in DATA_DRAWERS and any(w.upper() == "ALL" for w in words):
            src = dist / DATA_DRAWERS[words[1]]
            out += [(words[1] + "/" + f.name, words[2].rstrip("/") + "/" + f.name, f) for f in sorted(src.iterdir()) if f.is_file()]
    return out


def program_names(makefile_text):
    """Amiga programs the Makefile's `amiga` and `dist-base` rules build ($(BUILD)/amiga/<name>, objects skipped)."""
    names = []
    for line in makefile_text.splitlines():
        if line.startswith(("amiga:", "dist-base:")):
            for n in re.findall(r"\$\(BUILD\)/amiga/([^\s/]+)", line):
                if not n.endswith(".o") and n not in names:
                    names.append(n)
    return names


def unchecked(install_dos_text, programs, built_names):
    """Installed programs with no file in the build drawer: the run cannot compare them, so it names them
    rather than letting a partial build read as "nothing differs"."""
    built = set(built_names)
    return sorted({n for n, _ in install_map(install_dos_text, programs)} - built)


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
    """[(state, build name, amiga path)]; state same / DIFF / missing (path match ignores case: AmigaDOS).
    A build md5 of None means the file is the user's: present is `same`, whatever it holds."""
    low = {k.lower(): v for k, v in amiga.items()}
    return [("missing" if low.get(p.lower()) is None else "same" if build[n] is None or low[p.lower()] == build[n] else "DIFF", n, p)
            for n, p in entries]


def main(argv):
    bdir = pathlib.Path(argv[argv.index("--build") + 1]) if "--build" in argv else ROOT / "build/amiga"
    names = [f.name for f in bdir.iterdir() if f.is_file()]
    install = (ROOT / "dist/install.dos").read_text(errors="replace")
    entries = install_map(install, names + list(DIST_SOURCES))
    libs = lib_entries("--nofpu" in argv)
    have = [(n, p, src) for n, p, src in libs if src.is_file()]
    skipped = unchecked(install, program_names((ROOT / "Makefile").read_text(errors="replace")), names)
    build = {n: md5_file(ROOT / "dist" / n if n in DIST_SOURCES else bdir / n) for n, _ in entries}
    build.update({n: md5_file(src) for n, _, src in have})
    entries = entries + [(n, p) for n, p, _ in have]
    data = data_entries(install)
    build.update({n: None if n in USER_OWNED else md5_file(src) for n, _, src in data if src.is_file()})
    for n, _, src in data:
        if not src.is_file():
            print("%-8s %-28s not checked: no %s" % ("unbuilt", n, src))
            skipped.append(n)
    entries = entries + [(n, p) for n, p, src in data if src.is_file()]
    res = verdicts(entries, build, amiga_md5s([p for _, p in entries]))
    for state, n, p in res:
        print("%-8s %-28s %s" % (state, n, p))
    for n in skipped:
        print("%-8s %-28s not checked: no %s" % ("unbuilt", n, bdir / n))
    for n, _, src in libs:
        if not src.is_file():
            skipped.append(n)
            print("%-8s %-28s not checked: no %s" % ("unbuilt", n, src))
    bad = sum(1 for s, _, _ in res if s != "same")
    print("%d file(s), %d differ or missing, %d installed program(s) not checked (not built)" % (len(res), bad, len(skipped)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
