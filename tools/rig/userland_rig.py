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
     does not take a `<` redirect, so a case that needs a tty is not covered
     here but by the terminal cases (below);
  3. diffs each output against the expected one.

Terminal cases: pkgs/<pkg>/check/tty ("name<TAB>command", the command a
plain argv: no pipes or quotes) with the keys in check/keys/<name> run under
the ports' ptyrun (build/tools/ptyrun.amiga, `make tty-tools` there) on an
80x24 pseudo-terminal with TERM=vtcon (the ports' build/tools/terminfo).
ptyrun records the stream and the byte count at each key step; the Mac's
run of the same keys (`make host-<pkg>`) is the expected one. Both streams
are rendered by the engine (build/vtdump: text, colours and attributes) at
every step, and the screens and the program's exit status must match.
Verdict lines: tty:<name>.

  python3 tools/rig/userland_rig.py                 every package with check cases
  python3 tools/rig/userland_rig.py --only grep     one package (rerun even if it passed)
  python3 tools/rig/userland_rig.py --only grep --case recursive   one case
  python3 tools/rig/userland_rig.py --failed        the packages whose verdict is a failure
  python3 tools/rig/userland_rig.py --force         rerun the passed ones too
  python3 tools/rig/userland_rig.py --bash agent    the bash probes of tests/bash/probes/agent (below)
  python3 tools/rig/userland_rig.py --bash agent --case a001_pipefail_loop   one probe
  python3 tools/rig/userland_rig.py --bash agent --failed   the probes of AREA whose verdict is FAIL
                                                    (--case and --failed keep the other verdicts)

--bash AREA is a second case source (plan 2026-10-07-vsh-bash, V9): the probes
of tests/bash/probes/AREA run as `VTC:vsh <probe> [args]` in a fresh copy of
tests/bash/data (the same staging, run, verdict and fingerprint code), and
their output is compared with the one bash 5 gave on the Mac, written by
tools/bashdiff.py to build/bashdiff/expected/AREA/<name>.txt (stdout and a
last line "[exit N]"; run `make bashdiff` first). A probe with a .in file
gets its stdin as a here-document (EXEC takes no `<`). tests/bash/rig-skip.txt
("area/name<TAB>reason") lists the probes the rig does not run, and it skips the ones
tests/bash/divergences.txt records as differences from bash. Verdicts:
build/rig/userland/bash-AREA.verdict. Probes only the host can run (no vsh
binary change reaches them) belong in rig-skip.txt with the reason.

Verdicts: build/rig/userland/<pkg>.verdict (first line PASS or FAIL and a
fingerprint of the binaries, cases and expected files; then one line per case).
A package that passed with an unchanged fingerprint is skipped on the next run;
a case that passed is skipped when only some cases are rerun. Exit 0 when every
package that ran passed. The rig must be up (`rig.py start`, VTC: is
build/rig/vtc); `make build/amiga/vsh` first."""
import argparse, difflib, hashlib, os, pathlib, re, shlex, shutil, struct, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import paths

ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH_PROBES = ROOT / "tests/bash/probes"
BASH_DATA = ROOT / "tests/bash/data"
BASH_EXPECTED = ROOT / "build/bashdiff/expected"
BASH_SKIP = ROOT / "tests/bash/rig-skip.txt"
BASH_DIVERGENCES = ROOT / "tests/bash/divergences.txt"
VTC = paths.RIG / "vtc"
VERDICTS = paths.RIG / "userland"
PORTS = pathlib.Path(os.environ.get("UPTERM_PORTS", paths.repo("upterm-ports")))
COREUTILS = ROOT / "dist/gg/coreutils-5.2.1/bin"
SYSBIN = PORTS / "build/sysroot/SYS/UP-Term"
PTYRUN = PORTS / "build/tools/ptyrun.amiga"
TERMINFO = PORTS / "build/tools/terminfo"
VTDUMP = ROOT / "build/vtdump"


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def packages():
    """Packages with a check/cases or check/tty file, sorted."""
    return sorted({p.parent.parent.name for p in (PORTS / "pkgs").glob("*/check/cases")} |
                  {p.parent.parent.name for p in (PORTS / "pkgs").glob("*/check/tty")})


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
    files = list(bins) + [PORTS / "pkgs" / pkg / "check/cases", PORTS / "pkgs" / pkg / "check/tty"]
    files += sorted((PORTS / "build/expected" / pkg).glob("*.txt"))
    if tty_cases(pkg):
        files += [PTYRUN] + sorted((PORTS / "pkgs" / pkg / "check/keys").glob("*"))
        files += sorted((PORTS / "build/expected" / pkg).glob("*.stream*"))
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
            "unset LC_ALL LC_CTYPE SHLVL\n"   # SHLVL: the oracle starts bash from a clean environment; the rig nests two shells

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
    all_cases = cases(pkg) + [("tty:" + n, c) for n, c in tty_cases(pkg)]
    wanted = [c for c in all_cases if (not only_case or c[0] == only_case)]
    if only_case and not wanted:
        print("[ERROR] %s: no case %s" % (pkg, only_case))
        return False
    # a case that passed under this fingerprint stays passed unless it is asked for
    todo = [c for c in wanted if only_case or prior.get(c[0]) != "PASS"]
    results = dict(prior)
    plain = [c for c in todo if not c[0].startswith("tty:")]
    tty = [(n[4:], c) for n, c in todo if n.startswith("tty:")]
    stage(pkg, bins, plain)
    if tty:
        tty_stage(pkg, tty)
    for name, _cmd in todo:
        if name.startswith("tty:"):
            rc, out = run("VTC:vsh VTC:userland/%s/tty-%s.sh" % (pkg, name[4:]), 120)
            ok, diff = tty_compare(pkg, name[4:])
        else:
            rc, out = run_case(pkg, name)
            ok, diff = compare(pkg, name)
        results[name] = "PASS" if ok else "FAIL"
        print("%s %s/%s%s" % ("[OK]" if ok else "[FAIL]", pkg, name, "" if ok else " (agent rc %s %s)\n%s" % (rc, out.strip()[:200], diff)),
              flush=True)
    for name in [n for n in results if n not in dict(all_cases)]:
        del results[name]   # a case the package no longer has
    status = write_verdict(pkg, fp, results)
    print("%s: %s, %d of %d cases pass" % (pkg, status, sum(v == "PASS" for v in results.values()), len(results)))
    return status == "PASS"


def tty_cases(pkg):
    """(name, command) lines of pkgs/<pkg>/check/tty: the terminal cases."""
    f = PORTS / "pkgs" / pkg / "check/tty"
    out = []
    if f.exists():
        for line in f.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.startswith("#") and "\t" in line:
                name, cmd = line.split("\t", 1)
                out.append((name, cmd))
    return out


def tty_stage(pkg, wanted):
    """ptyrun, the terminfo, the keys and one vsh script per terminal case into VTC:."""
    shutil.copyfile(PTYRUN, VTC / "userland/bin/ptyrun")
    shutil.rmtree(VTC / "userland/terminfo", ignore_errors=True)
    shutil.copytree(TERMINFO, VTC / "userland/terminfo")
    keys = VTC / "userland" / pkg / "keys"
    keys.mkdir(parents=True, exist_ok=True)
    for name, cmd in wanted:
        shutil.copyfile(PORTS / "pkgs" / pkg / "check/keys" / name, keys / name)
        (VTC / "userland" / pkg / ("tty-" + name + ".sh")).write_text(
            "cd VTC:userland/work/" + pkg + "\n"
            "export PATH=/VTC/userland/bin:$PATH\n"
            "unset LC_ALL LC_CTYPE SHLVL\n"
            "export LANG=C TERM=vtcon TERMINFO=/VTC/userland/terminfo\n"
            "ptyrun -t 30 VTC:userland/%s/keys/%s VTC:out/%s/%s.stream %s >VTC:out/%s/%s.txt 2>VTC:out/%s/%s.err\n"
            % (pkg, name, pkg, name, cmd, pkg, name, pkg, name), encoding="latin-1")


def render(stream, upto):
    """The engine's screen (text, then colours and attributes) after the first `upto` bytes."""
    return subprocess.run([str(VTDUMP), "80", "24"], input=stream[:upto], capture_output=True).stdout.decode("utf-8", "replace")


def marks(f):
    """[(step, bytes)] of a ptyrun .marks file (not the end line)."""
    out = []
    for l in (f.read_text().splitlines() if f.exists() else []):
        a, b = l.split()
        if a != "end":
            out.append((a, int(b)))
    return out


def tty_compare(pkg, name):
    """(ok, diff text) of a terminal case: exit status, then the screen at every key step."""
    exp, got = PORTS / "build/expected" / pkg, VTC / "out" / pkg
    if not (exp / (name + ".stream")).exists():
        return False, "no expected stream (make host-%s in the ports repo)" % pkg
    if not (got / (name + ".stream")).exists():
        return False, "no stream captured: %s" % ((got / (name + ".err")).read_text(encoding="latin-1").strip()[:200]
                                                   if (got / (name + ".err")).exists() else "")
    a = (exp / (name + ".txt")).read_text(encoding="latin-1").strip()
    b = (got / (name + ".txt")).read_text(encoding="latin-1").strip() if (got / (name + ".txt")).exists() else ""
    if a != b:
        return False, "program status: expected %r, rig %r" % (a, b)
    es, gs = (exp / (name + ".stream")).read_bytes(), (got / (name + ".stream")).read_bytes()
    em, gm = marks(exp / (name + ".stream.marks")), marks(got / (name + ".stream.marks"))
    if len(em) != len(gm):
        return False, "key steps: expected %d, rig %d" % (len(em), len(gm))
    for (step, eb), (_s, gb) in zip(em, gm):
        x, y = render(es, eb), render(gs, gb)
        if x != y:
            d = list(difflib.unified_diff(x.splitlines()[:24], y.splitlines()[:24], "expected", "rig", lineterm=""))
            return False, "screen before key step %s differs%s\n%s" % (
                step, "" if d else " (colours or attributes only)", "\n".join(d[:20]))
    return True, ""


def bash_cases(area):
    """Probe names of tests/bash/probes/AREA the rig runs: not the ones rig-skip.txt names, nor the
    differences from bash divergences.txt records (their output differs from the oracle by decision)."""
    skip = set()
    for f in (BASH_SKIP, BASH_DIVERGENCES):
        if f.exists():
            skip |= {l.split("\t")[0] for l in f.read_text().splitlines() if l.strip() and not l.startswith("#")}
    return [p.stem for p in sorted((BASH_PROBES / area).glob("*.sh")) if "%s/%s" % (area, p.stem) not in skip]


def bash_wrapper(area, name):
    """The script the rig runs for one probe: vsh on the probe in a fresh data copy, then "[exit N]"."""
    pdir = BASH_PROBES / area
    args = (pdir / (name + ".args")).read_text().strip() if (pdir / (name + ".args")).exists() else ""
    work = "VTC:userland/bashwork/%s_%s" % (area, name)
    fl = pdir / (name + ".flags")
    flags = shlex.split(fl.read_text()) if fl.exists() else []
    target, stdin_text = "VTC:userland/bash/%s/%s.sh" % (area, name), None
    # <name>.flags as tools/bashdiff.py reads them: options before the probe; last flag -c / -ec: the
    # probe's text is the command string; last flag -s: the text is the shell's input; last flag PTY (a
    # host pseudo-terminal in bashdiff): the rig feeds the text as with -s (the console is no pty)
    if flags and flags[-1] == "PTY":
        flags[-1] = "-s"
    if flags and re.match(r"^-[a-zA-Z]*c$", flags[-1]):
        target = shlex.quote((pdir / (name + ".sh")).read_text(encoding="latin-1"))
    elif flags and flags[-1] == "-s":
        target, stdin_text = "", (pdir / (name + ".sh")).read_text(encoding="latin-1")
    cmd = "VTC:vsh %s %s %s" % (" ".join(flags), target, args)
    text = ("cd " + work + "\n"
            "export PATH=/VTC/userland/bin:$PATH\n"
            "unset LC_ALL LC_CTYPE SHLVL\n"   # SHLVL: the oracle starts bash from a clean environment; the rig nests two shells

            "export LANG=C TZ=UTC\n")
    inp = pdir / (name + ".in")
    if stdin_text is not None:
        text += "{ " + cmd + " <<'BASHDIFF_IN'\n" + stdin_text + ("" if stdin_text.endswith("\n") else "\n") + "BASHDIFF_IN\n} 2>VTC:out/bash-%s/%s.err\n" % (area, name)
    elif inp.exists():
        text += "{ " + cmd + " <<'BASHDIFF_IN'\n" + inp.read_text(encoding="latin-1") + "BASHDIFF_IN\n} 2>VTC:out/bash-%s/%s.err\n" % (area, name)
    else:
        text += "{ " + cmd + "\n} 2>VTC:out/bash-%s/%s.err\n" % (area, name)
    return text + "echo \"[exit $?]\"\n"


def stage_bash(area, wanted):
    """The vsh binary, the probes (as they are), a wrapper and a fresh data copy per wanted probe into VTC:."""
    pkg = "bash-" + area
    (VTC / "userland").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / "build/amiga/vsh", VTC / "vsh")
    # the ports built so far (grep) and the kit's coreutils (dist/gg: cat, tr, env, mkdir, wc ...;
    # the rig's system disk has none), on PATH as /VTC/userland/bin
    (VTC / "userland/bin").mkdir(parents=True, exist_ok=True)
    for tool in ("grep", "egrep", "fgrep"):
        if (SYSBIN / "bin" / tool).exists():
            shutil.copyfile(SYSBIN / "bin" / tool, VTC / "userland/bin" / tool)
    for f in sorted(COREUTILS.iterdir()):
        if f.is_file():
            shutil.copyfile(f, VTC / "userland/bin" / f.name)
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


def run_watched(pkg, name, limit=90):
    """One probe in the background (Run), so a probe that never ends cannot wedge amiagent: the host
    waits for its done-file up to limit seconds. A probe that did not end: 'HANG', and the rig is
    rebooted (a vsh blocked in a PIPE: read ignores Ctrl-C) so the next probe has a clean machine."""
    done = VTC / "out" / pkg / (name + ".done")
    done.unlink(missing_ok=True)
    launcher = VTC / "userland" / pkg / (name + ".run")
    launcher.write_text("VTC:vsh VTC:userland/%s/%s.sh >VTC:out/%s/%s.txt\necho done >VTC:out/%s/%s.done\n"
                        % (pkg, name, pkg, name, pkg, name), encoding="latin-1")
    run("Run >NIL: VTC:vsh VTC:userland/%s/%s.run" % (pkg, name), 20)
    end = time.time() + limit
    while time.time() < end:
        if done.exists():
            return "ended"
        time.sleep(0.5)
    rig = [sys.executable, str(pathlib.Path(__file__).with_name("rig.py"))]
    subprocess.run(rig + ["stop"], capture_output=True, timeout=120)
    subprocess.run(rig + ["start", "--max"], capture_output=True, timeout=400)
    return "HANG (no end in %d s; rig rebooted)" % limit


def check_bash(area, only_case, failed=False):
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
    # --case and --failed rerun those probes only and keep the other probes' recorded verdicts
    # (the ones that passed on an older binary stay PASS until a full run: say so when it matters)
    prior = old[2] if old and (old[1] == fp or only_case or failed) else {}
    wanted = [n for n in names if (not only_case or n == only_case)]
    todo = [n for n in wanted if only_case or prior.get(n) != "PASS"]
    results = dict(prior)
    stage_bash(area, todo)
    # /tmp is the volume TMP: (vsh and ixemul); the kit's Install assigns it to T: when it is missing,
    # the rig never ran Install: the same assign here (probes write ${TMPDIR:-/tmp}/name)
    if todo and run("Assign >NIL: TMP: EXISTS", 20)[0] != 0:
        run("Assign TMP: T:", 20)
    for name in todo:
        how = run_watched(pkg, name)
        ok, diff = compare(pkg, name, BASH_EXPECTED / area / (name + ".txt"), "make bashdiff ONLY=%s" % area)
        ok = ok and how == "ended"
        results[name] = "PASS" if ok else "FAIL"
        print("%s %s/%s%s" % ("[OK]" if ok else "[FAIL]", pkg, name, "" if ok else " (%s)\n%s" % (how, diff)),
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
        return 0 if check_bash(a.bash, a.case, a.failed) else 1
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
