#!/usr/bin/env python3
"""userland_rig.py -- the userland ports on the rig (unix-tool-ports plan, item
0.5): each package's check cases run through vsh and are compared with the
outputs the same upstream version gives on the Mac.

The ports repo (UPTERM_PORTS, default $UPTERM_ROOT/upterm-ports) holds, per package,
pkgs/<pkg>/check/cases ("name<TAB>command" lines) and check/data (the files the
cases read); `make host-<pkg>` there writes build/expected/<pkg>/<name>.txt
(stdout and a last line "[exit N]"; stderr is not compared). `make <pkg>`
installs the Amiga binaries under build/sysroot/SYS/UP-Term/bin.

For each package this script
  1. copies the binaries (<pkg>_BINS of its recipe.mk) to VTC:userland/bin and
     the check data to VTC:userland/work/<pkg>;
  2. writes one vsh script per case (VTC:userland/<pkg>/<name>.sh: cd to the
     data, PATH with the binaries first, LANG=C, the case's command, then
     "[exit $?]") and runs it as `VTC:vsh <script>` with stdout to
     VTC:out/<pkg>/<name>.txt. There is no terminal window: the agent's EXEC
     does not take a `<` redirect, so a case that needs a tty is not covered;
  3. diffs each output against the expected one.

  python3 tools/rig/userland_rig.py                 every package with check cases
  python3 tools/rig/userland_rig.py --only grep     one package (rerun even if it passed)
  python3 tools/rig/userland_rig.py --only grep --case recursive   one case
  python3 tools/rig/userland_rig.py --failed        the packages whose verdict is a failure
  python3 tools/rig/userland_rig.py --force         rerun the passed ones too
  python3 tools/rig/userland_rig.py --bash agent    the bash probes of tests/bash/probes/agent (below)
  python3 tools/rig/userland_rig.py --bash agent --case a001_pipefail_loop   one probe

--bash AREA is a second case source (plan 2026-10-07-vsh-bash, V9): the probes
of tests/bash/probes/AREA run as `VTC:vsh <probe> [args]` in a fresh copy of
tests/bash/data (the same staging, run, verdict and fingerprint code), and
their output is compared with the one bash 5 gave on the Mac, written by
tools/bashdiff.py to build/bashdiff/expected/AREA/<name>.txt (stdout and a
last line "[exit N]"; run `make bashdiff` first). A probe with a .in file
gets its stdin as a here-document (EXEC takes no `<`). tests/bash/rig-skip.txt
("area/name<TAB>reason") lists the probes the rig does not run. Verdicts:
build/rig/userland/bash-AREA.verdict. Probes only the host can run (no vsh
binary change reaches them) belong in rig-skip.txt with the reason.

Verdicts: build/rig/userland/<pkg>.verdict (first line PASS or FAIL and a
fingerprint of the binaries, cases and expected files; then one line per case).
A package that passed with an unchanged fingerprint is skipped on the next run;
a case that passed is skipped when only some cases are rerun. Exit 0 when every
package that ran passed. The rig must be up (`rig.py start`, VTC: is
build/rig/vtc); `make build/amiga/vsh` first."""
import argparse, difflib, hashlib, os, pathlib, re, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import paths

ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH_PROBES = ROOT / "tests/bash/probes"
BASH_DATA = ROOT / "tests/bash/data"
BASH_EXPECTED = ROOT / "build/bashdiff/expected"
BASH_SKIP = ROOT / "tests/bash/rig-skip.txt"
VTC = ROOT / "build/rig/vtc"
VERDICTS = ROOT / "build/rig/userland"
PORTS = pathlib.Path(os.environ.get("UPTERM_PORTS", paths.repo("upterm-ports")))
SYSBIN = PORTS / "build/sysroot/SYS/UP-Term"


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def packages():
    """Packages with a check/cases file, sorted."""
    return sorted(p.parent.parent.name for p in (PORTS / "pkgs").glob("*/check/cases"))


def cases(pkg):
    """(name, command) lines of pkgs/<pkg>/check/cases, as tools/run-cases.sh reads them."""
    out = []
    for line in (PORTS / "pkgs" / pkg / "check/cases").read_text(encoding="latin-1").splitlines():
        name, _, cmd = line.partition("\t")
        if name and not name.startswith("#"):
            out.append((name, cmd))
    return out


def binaries(pkg):
    """The installed programs: <pkg>_BINS of its recipe.mk, relative to the sysroot's UP-Term."""
    m = re.search(r"^%s_BINS\s*:?=\s*(.*)$" % re.escape(pkg), (PORTS / "pkgs" / pkg / "recipe.mk").read_text(), re.M)
    return [SYSBIN / b for b in (m.group(1).split() if m else [])]


def fingerprint(pkg, bins):
    h = hashlib.sha256()
    files = list(bins) + [PORTS / "pkgs" / pkg / "check/cases"]
    files += sorted((PORTS / "build/expected" / pkg).glob("*.txt"))
    for f in files:
        h.update(f.name.encode() + (f.read_bytes() if f.exists() else b"<missing>"))
    return h.hexdigest()[:16]


def read_verdict(pkg):
    """(status, fingerprint, {case: PASS|FAIL}) or None."""
    f = VERDICTS / (pkg + ".verdict")
    if not f.exists():
        return None
    lines = f.read_text().splitlines()
    head = lines[0].split() if lines else []
    if len(head) < 2:
        return None
    return head[0], head[1], dict(l.split("\t", 1) for l in lines[1:] if "\t" in l)


def write_verdict(pkg, fp, results):
    VERDICTS.mkdir(parents=True, exist_ok=True)
    status = "PASS" if results and all(v == "PASS" for v in results.values()) else "FAIL"
    (VERDICTS / (pkg + ".verdict")).write_text(
        "%s %s\n%s" % (status, fp, "".join("%s\t%s\n" % kv for kv in results.items())))
    return status


def script_text(pkg, name, cmd):
    """The vsh script of one case: the same environment tools/run-cases.sh gives sh."""
    return ("cd VTC:userland/work/" + pkg + "\n"
            "export PATH=/VTC/userland/bin:$PATH\n"
            "unset LC_ALL LC_CTYPE\n"
            "export LANG=C\n"
            "{ " + cmd + "\n} 2>VTC:out/" + pkg + "/" + name + ".err\n"
            "echo \"[exit $?]\"\n")


def stage(pkg, bins, wanted):
    """Binaries, data and one script per wanted case into VTC:."""
    (VTC / "userland/bin").mkdir(parents=True, exist_ok=True)
    for b in bins:
        shutil.copyfile(b, VTC / "userland/bin" / b.name)
    work = VTC / "userland/work" / pkg
    shutil.rmtree(work, ignore_errors=True)
    data = PORTS / "pkgs" / pkg / "check/data"
    if data.is_dir():
        shutil.copytree(data, work)
    else:
        work.mkdir(parents=True)
    scripts = VTC / "userland" / pkg
    shutil.rmtree(scripts, ignore_errors=True)
    scripts.mkdir(parents=True)
    out = VTC / "out" / pkg
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    for name, cmd in wanted:
        (scripts / (name + ".sh")).write_text(script_text(pkg, name, cmd), encoding="latin-1")


def run_case(pkg, name):
    rc, out = run("VTC:vsh VTC:userland/%s/%s.sh >VTC:out/%s/%s.txt" % (pkg, name, pkg, name), 90)
    return rc, out


def compare(pkg, name, exp=None, hint=None):
    """(ok, diff text) of the case's output against the expected one."""
    exp = exp or PORTS / "build/expected" / pkg / (name + ".txt")
    got = VTC / "out" / pkg / (name + ".txt")
    if not exp.exists():
        return False, "no expected output (%s)" % (hint or "make host-%s in the ports repo" % pkg)
    if not got.exists():
        return False, "no output captured"
    a = exp.read_text(encoding="latin-1").splitlines()
    b = got.read_text(encoding="latin-1").splitlines()
    if a == b:
        return True, ""
    return False, "\n".join(list(difflib.unified_diff(a, b, "expected", "rig", lineterm=""))[:20])


def check_package(pkg, only_case):
    bins = binaries(pkg)
    missing = [b.name for b in bins if not b.exists()]
    if not bins or missing:
        print("[ERROR] %s: no binary (%s): make %s in the ports repo" % (pkg, ", ".join(missing) or "no _BINS", pkg))
        return False
    fp = fingerprint(pkg, bins)
    old = read_verdict(pkg)
    prior = old[2] if old and old[1] == fp else {}
    wanted = [c for c in cases(pkg) if (not only_case or c[0] == only_case)]
    if only_case and not wanted:
        print("[ERROR] %s: no case %s" % (pkg, only_case))
        return False
    # a case that passed under this fingerprint stays passed unless it is asked for
    todo = [c for c in wanted if only_case or prior.get(c[0]) != "PASS"]
    results = dict(prior)
    stage(pkg, bins, todo)
    for name, _cmd in todo:
        rc, out = run_case(pkg, name)
        ok, diff = compare(pkg, name)
        results[name] = "PASS" if ok else "FAIL"
        print("%s %s/%s%s" % ("[OK]" if ok else "[FAIL]", pkg, name, "" if ok else " (agent rc %s %s)\n%s" % (rc, out.strip()[:200], diff)),
              flush=True)
    for name in [n for n in results if n not in dict(cases(pkg))]:
        del results[name]   # a case the package no longer has
    status = write_verdict(pkg, fp, results)
    print("%s: %s, %d of %d cases pass" % (pkg, status, sum(v == "PASS" for v in results.values()), len(results)))
    return status == "PASS"


def bash_cases(area):
    """Probe names of tests/bash/probes/AREA the rig runs (rig-skip.txt names the others)."""
    skip = set()
    if BASH_SKIP.exists():
        skip = {l.split("\t")[0] for l in BASH_SKIP.read_text().splitlines() if l.strip() and not l.startswith("#")}
    return [p.stem for p in sorted((BASH_PROBES / area).glob("*.sh")) if "%s/%s" % (area, p.stem) not in skip]


def bash_wrapper(area, name):
    """The script the rig runs for one probe: vsh on the probe in a fresh data copy, then "[exit N]"."""
    pdir = BASH_PROBES / area
    args = (pdir / (name + ".args")).read_text().strip() if (pdir / (name + ".args")).exists() else ""
    work = "VTC:userland/bashwork/%s_%s" % (area, name)
    cmd = "VTC:vsh VTC:userland/bash/%s/%s.sh %s" % (area, name, args)
    text = ("cd " + work + "\n"
            "export PATH=/UP-Term/bin:$PATH\n"
            "unset LC_ALL LC_CTYPE\n"
            "export LANG=C TZ=UTC\n")
    inp = pdir / (name + ".in")
    if inp.exists():
        text += "{ " + cmd + " <<'BASHDIFF_IN'\n" + inp.read_text(encoding="latin-1") + "BASHDIFF_IN\n} 2>VTC:out/bash-%s/%s.err\n" % (area, name)
    else:
        text += "{ " + cmd + "\n} 2>VTC:out/bash-%s/%s.err\n" % (area, name)
    return text + "echo \"[exit $?]\"\n"


def stage_bash(area, wanted):
    """The vsh binary, the probes (as they are), a wrapper and a fresh data copy per wanted probe into VTC:."""
    pkg = "bash-" + area
    (VTC / "userland").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / "build/amiga/vsh", VTC / "vsh")
    pdest = VTC / "userland/bash" / area
    shutil.rmtree(pdest, ignore_errors=True)
    pdest.mkdir(parents=True)
    scripts = VTC / "userland" / pkg
    shutil.rmtree(scripts, ignore_errors=True)
    scripts.mkdir(parents=True)
    out = VTC / "out" / pkg
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    for name in wanted:
        shutil.copyfile(BASH_PROBES / area / (name + ".sh"), pdest / (name + ".sh"))
        work = VTC / "userland/bashwork" / ("%s_%s" % (area, name))
        shutil.rmtree(work, ignore_errors=True)
        shutil.copytree(BASH_DATA, work)
        (scripts / (name + ".sh")).write_text(bash_wrapper(area, name), encoding="latin-1")


def bash_fingerprint(area):
    h = hashlib.sha256()
    vsh = ROOT / "build/amiga/vsh"
    h.update(vsh.read_bytes() if vsh.exists() else b"<missing>")
    for p in sorted((BASH_PROBES / area).glob("*")):
        h.update(p.name.encode() + p.read_bytes())
    for f in sorted(BASH_DATA.rglob("*")):
        if f.is_file():
            h.update(str(f.relative_to(BASH_DATA)).encode() + f.read_bytes())
    for f in sorted((BASH_EXPECTED / area).glob("*.txt")):
        h.update(f.name.encode() + f.read_bytes())
    return h.hexdigest()[:16]


def check_bash(area, only_case):
    pkg = "bash-" + area
    if not (BASH_PROBES / area).is_dir():
        print("[ERROR] no area %s in %s" % (area, BASH_PROBES))
        return False
    if not (ROOT / "build/amiga/vsh").exists():
        print("[ERROR] build/amiga/vsh missing: make build/amiga/vsh")
        return False
    names = bash_cases(area)
    if only_case and only_case not in names:
        print("[ERROR] %s: no case %s (or it is in rig-skip.txt)" % (pkg, only_case))
        return False
    fp = bash_fingerprint(area)
    old = read_verdict(pkg)
    prior = old[2] if old and old[1] == fp else {}
    wanted = [n for n in names if (not only_case or n == only_case)]
    todo = [n for n in wanted if only_case or prior.get(n) != "PASS"]
    results = dict(prior)
    stage_bash(area, todo)
    for name in todo:
        rc, out = run("VTC:vsh VTC:userland/%s/%s.sh >VTC:out/%s/%s.txt" % (pkg, name, pkg, name), 90)
        ok, diff = compare(pkg, name, BASH_EXPECTED / area / (name + ".txt"), "make bashdiff ONLY=%s" % area)
        results[name] = "PASS" if ok else "FAIL"
        print("%s %s/%s%s" % ("[OK]" if ok else "[FAIL]", pkg, name, "" if ok else " (agent rc %s %s)\n%s" % (rc, out.strip()[:200], diff)),
              flush=True)
    for name in [n for n in results if n not in names]:
        del results[name]
    status = write_verdict(pkg, fp, results)
    print("%s: %s, %d of %d probes identical to bash" % (pkg, status, sum(v == "PASS" for v in results.values()), len(results)))
    return status == "PASS"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--bash", metavar="AREA", help="the bash probes of tests/bash/probes/AREA instead of the ports' cases")
    ap.add_argument("--only", nargs="+", metavar="PKG", help="these packages, even if they passed")
    ap.add_argument("--case", help="one case (with one --only package)")
    ap.add_argument("--failed", action="store_true", help="the packages whose verdict is FAIL")
    ap.add_argument("--force", action="store_true", help="rerun passed packages too")
    a = ap.parse_args()
    if a.bash:
        VERDICTS.mkdir(parents=True, exist_ok=True)
        if a.force:
            (VERDICTS / ("bash-" + a.bash + ".verdict")).unlink(missing_ok=True)
        return 0 if check_bash(a.bash, a.case) else 1
    if a.case and not (a.only and len(a.only) == 1):
        ap.error("--case needs exactly one --only package")
    names = a.only or packages()
    if a.failed:
        names = [p for p in names if (read_verdict(p) or ("",))[0] == "FAIL"]
    ok = True
    for pkg in names:
        if pkg not in packages():
            print("[ERROR] %s: no pkgs/%s/check/cases in %s" % (pkg, pkg, PORTS))
            ok = False
            continue
        v = read_verdict(pkg)
        if not (a.only or a.force or a.failed) and v and v[0] == "PASS" and v[1] == fingerprint(pkg, binaries(pkg)):
            print("[INFO] %s: passed already (same binaries and cases), skipped" % pkg)
            continue
        if a.only or a.force:
            VERDICTS.mkdir(parents=True, exist_ok=True)
            if not a.case and v:
                (VERDICTS / (pkg + ".verdict")).unlink()   # explicit: every case again
        ok &= check_package(pkg, a.case)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
