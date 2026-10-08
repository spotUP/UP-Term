#!/usr/bin/env python3
"""bashdiff.py -- vsh against bash 5, probe by probe (plan 2026-10-07-vsh-bash, P0).

Every probe tests/bash/probes/<area>/<name>.sh (optional <name>.in = stdin,
<name>.args = argv, shell-quoted) runs under the oracle bash and under
build/vsh_host, in a fresh copy of tests/bash/data, with a clean environment
(env -i PATH=/usr/bin:/bin HOME=<tmp> LANG=C LC_ALL=C TZ=UTC TMPDIR=<tmp>) and
a 5 s timeout. stdout bytes and the exit status are compared; stderr is not (a
probe that needs it writes 2>&1). "Identical" is a pass.

The oracle is $BASH_ORACLE (default /opt/homebrew/bin/bash). /bin/bash on macOS
is 3.2: the runner refuses any oracle whose BASH_VERSINFO[0] is below 5.

A probe with <name>.hits also runs under `vsh_host --hits`; the file lists counters of shell/sh_hits.h
(NAME>=N or NAME=N) that must hold: identical output that never ran the feature proves nothing.

Files (tests/bash/):
  divergences.txt   probe-id TAB class TAB reason; class n/a-amiga | documented | later:Vnn
  ratchet.txt       probe ids that must stay identical
Each probe is in exactly one of them. A later:Vnn[,Vmm] entry whose items are all ticked in
the plan's checklist is an error (the item is done: the probe belongs in the
ratchet), and every n/a-amiga and documented entry must be named (by probe id) in
the "Differences from bash" block of the VSH section of dist/README.txt.

The oracle's output is written in userland_rig.py's format (stdout, then a last
line "[exit N]") to build/bashdiff/expected/<area>/<name>.txt for the rig; diffs
go to build/bashdiff/diff/. Verdicts: build/bashdiff/<area>.verdict (first line
PASS|FAIL and a fingerprint of vsh_host and the oracle version, then one line
per probe: name TAB PASS|FAIL TAB probe-hash). A passed probe whose fingerprint
and hash are unchanged is skipped on the next run.

  tools/bashdiff.py                    every area
  tools/bashdiff.py --only expand      one area
  tools/bashdiff.py --probe subst      one probe (name or area/name), rerun
  tools/bashdiff.py --failed           the probes whose verdict is FAIL
  tools/bashdiff.py --force            every probe again
  tools/bashdiff.py --leak             every ratchet probe under build/vsh_host_leak (counting
                                       allocator, no sanitizers) with VSH_LEAKCHECK=1: fails when a probe
                                       leaves a block live after sh_shell_free (V44);
                                       --probe NAME limits it to one probe
  tools/bashdiff.py --gate             the ratchet only, plus the list checks (make test);
                                       "[INFO] no bash 5" and a skip when the oracle is missing
Exit 0 when the gate passes (or, without --gate, when no ratchet probe fails and
no probe is unclassified). Prints the summary and the names that need attention."""
import argparse, concurrent.futures, difflib, hashlib, os, pathlib, re, shlex, shutil, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
PROBES = ROOT / "tests/bash/probes"
DATA = ROOT / "tests/bash/data"
DIVERGENCES = ROOT / "tests/bash/divergences.txt"
RATCHET = ROOT / "tests/bash/ratchet.txt"
PLAN = ROOT / "thoughts/shared/plans/2026-10-07-vsh-bash.md"
README = ROOT / "dist/README.txt"
OUT = ROOT / "build/bashdiff"
VSH = ROOT / "build/vsh_host"
VSH_LEAK = ROOT / "build/vsh_host_leak"
LEAK_BASE = 0  # blocks the counting allocator may find live after sh_shell_free
ORACLE = os.environ.get("BASH_ORACLE", "/opt/homebrew/bin/bash")
PATH = "/usr/bin:/bin"
TIMEOUT = 5
CLASSES = re.compile(r"^(n/a-amiga|documented|later:V\d+(,V\d+)*)$")


class Skip(Exception):
    pass


def oracle_version():
    """The oracle's BASH_VERSION; Skip when it is missing, SystemExit when it is below 5."""
    if not os.access(ORACLE, os.X_OK):
        raise Skip()
    r = subprocess.run([ORACLE, "-c", 'printf "%s %s" "${BASH_VERSINFO[0]}" "$BASH_VERSION"'],
                       capture_output=True, text=True, env={"PATH": PATH})
    major, _, ver = r.stdout.partition(" ")
    if not major.isdigit() or int(major) < 5:
        print("[ERROR] oracle %s is bash %s: bashdiff needs bash 5 or newer (BASH_ORACLE=...)" % (ORACLE, ver or "?"))
        raise SystemExit(2)
    return ver


def probes():
    """{id: pathlib.Path of the .sh} for every probe, id = area/name."""
    return {"%s/%s" % (p.parent.name, p.stem): p for p in sorted(PROBES.glob("*/*.sh"))}


def read_pairs(path, cols):
    out = []
    if path.exists():
        for n, line in enumerate(path.read_text().splitlines(), 1):
            if line.strip() and not line.startswith("#"):
                f = line.split("\t")
                out.append((n, f + [""] * (cols - len(f))))
    return out


def ticked_items():
    """Plan checklist items ticked: {'V12', ...}."""
    if not PLAN.exists():
        return set()
    return set(re.findall(r"^- \[x\] (V\d+)\b", PLAN.read_text(), re.M))


def readme_block():
    """The text of dist/README.txt's "Differences from bash" block (to the next heading)."""
    if not README.exists():
        return ""
    lines = README.read_text(encoding="utf-8", errors="replace").splitlines()
    for i, l in enumerate(lines):
        if l.strip() == "Differences from bash":
            j = i + 1
            while j < len(lines) and (not lines[j] or lines[j][0] == " "):
                j += 1
            return "\n".join(lines[i:j])
    return ""


def lists(allp):
    """(divergences {id: (class, reason)}, ratchet set, [errors])."""
    errs = []
    div, rat = {}, set()
    ticked = ticked_items()
    for n, (pid, cls, why) in read_pairs(DIVERGENCES, 3):
        loc = "divergences.txt:%d" % n
        if pid not in allp:
            errs.append("%s: no probe %s" % (loc, pid))
        elif pid in div:
            errs.append("%s: %s listed twice" % (loc, pid))
        elif not CLASSES.match(cls):
            errs.append("%s: %s: class %r is not n/a-amiga, documented or later:Vnn" % (loc, pid, cls))
        elif cls.startswith("later:") and set(cls[6:].split(",")) <= ticked:
            errs.append("%s: %s: %s but every item is ticked in the plan (move the probe to ratchet.txt)" % (loc, pid, cls))
        elif not why.strip():
            errs.append("%s: %s has no reason" % (loc, pid))
        else:
            div[pid] = (cls, why)
    for n, (pid, *_rest) in read_pairs(RATCHET, 1):
        if pid not in allp:
            errs.append("ratchet.txt:%d: no probe %s" % (n, pid))
        else:
            rat.add(pid)
    for pid in sorted(rat & set(div)):
        errs.append("%s is in ratchet.txt and in divergences.txt" % pid)
    for pid in sorted(set(allp) - rat - set(div)):
        errs.append("%s is in neither ratchet.txt nor divergences.txt (unclassified)" % pid)
    block = readme_block()
    for pid, (cls, _why) in sorted(div.items()):
        if cls in ("n/a-amiga", "documented") and pid not in block:
            errs.append("%s (%s) is not named in dist/README.txt, block \"Differences from bash\"" % (pid, cls))
    return div, rat, errs


def probe_hash(p):
    h = hashlib.sha256()
    for ext in (".sh", ".in", ".args", ".flags", ".hits"):
        f = p.with_suffix(ext)
        h.update(ext.encode() + (f.read_bytes() if f.exists() else b"-"))
    for f in sorted(DATA.rglob("*")):
        if f.is_file():
            h.update(str(f.relative_to(DATA)).encode() + f.read_bytes())
    return h.hexdigest()[:12]


def run_one(shell_cmd, p, base, hits=False, leak=False):
    """(stdout bytes, status, stderr bytes) of one shell on probe p, in a fresh work dir under base.
    hits: vsh_host runs with --hits and its stderr (the counters) is returned; else stderr is dropped."""
    work, tmp = base / "work", base / "tmp"
    for d in (work, tmp, base / "home"):
        shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(DATA, work, symlinks=True)
    tmp.mkdir()
    (base / "home").mkdir()
    args = shlex.split(p.with_suffix(".args").read_text()) if p.with_suffix(".args").exists() else []
    stdin = open(p.with_suffix(".in"), "rb") if p.with_suffix(".in").exists() else subprocess.DEVNULL
    env = {"PATH": PATH, "HOME": str(base / "home"), "LANG": "C", "LC_ALL": "C", "TZ": "UTC", "TMPDIR": str(tmp)}
    if leak:
        env["VSH_LEAKCHECK"] = "1"
    flags = shlex.split(p.with_suffix(".flags").read_text()) if p.with_suffix(".flags").exists() else []
    # <name>.flags: options before the probe. Last flag -c / -ec ...: the probe's text is the
    # command string (args after it are $0 and the positionals); last flag -s: the text is
    # fed on stdin (args are the positionals); otherwise the probe is a file as always.
    target = [str(p)]
    if flags and flags[-1] == "PTY":
        # last flag PTY (V94): the text is typed into a pseudo-terminal that is the shell's stdin, a line
        # every 50 ms, then ^D; stdout stays a pipe (bash -i's prompts and readline's echo go to stderr)
        return run_pty(shell_cmd + (["--hits"] if hits else []) + flags[:-1] + args, p.read_bytes(), work, env,
                       hits or leak)
    if flags and re.match(r"^-[a-zA-Z]*c$", flags[-1]):
        target = [p.read_text()]
    elif flags and flags[-1] == "-s":
        target = []
        stdin = p.read_bytes()
    try:
        r = subprocess.run(shell_cmd + (["--hits"] if hits else []) + flags + target + args,
                           stdin=None if isinstance(stdin, bytes) else stdin,
                           input=stdin if isinstance(stdin, bytes) else None, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE if hits or leak else subprocess.DEVNULL,
                           cwd=work, env=env, timeout=TIMEOUT, start_new_session=True)
        return r.stdout, r.returncode if r.returncode >= 0 else 128 - r.returncode, r.stderr or b""
    except subprocess.TimeoutExpired as e:
        return (e.stdout or b""), "timeout", b""
    finally:
        if not isinstance(stdin, bytes) and hasattr(stdin, "close"):
            stdin.close()


def run_pty(cmd, text, work, env, want_err=False):
    """cmd with a pseudo-terminal as stdin, text typed into it line by line, then ^D: (stdout, status,
    stderr when want_err, else b"")."""
    import pty, threading, time
    master, slave = pty.openpty()
    proc = subprocess.Popen(cmd, stdin=slave, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE if want_err else subprocess.DEVNULL, cwd=work,
                            env=env, start_new_session=True)
    os.close(slave)
    out, err = [], []
    reader = threading.Thread(target=lambda: out.append(proc.stdout.read()))
    reader.start()
    if want_err:
        ereader = threading.Thread(target=lambda: err.append(proc.stderr.read()))
        ereader.start()

    def drain():  # the terminal's echo, read so the shell never blocks on it
        try:
            while os.read(master, 4096):
                pass
        except OSError:
            pass
    threading.Thread(target=drain, daemon=True).start()
    try:
        for line in text.splitlines(True) + [b"\x04"]:
            time.sleep(0.05)
            if proc.poll() is not None:
                break
            os.write(master, line)
        proc.wait(timeout=TIMEOUT)
        status = proc.returncode if proc.returncode >= 0 else 128 - proc.returncode
    except (subprocess.TimeoutExpired, OSError):
        proc.kill()
        proc.wait()
        status = "timeout"
    reader.join()
    if want_err:
        ereader.join()
    os.close(master)
    return out[0] if out else b"", status, (err[0] if err else b"")


def hits_check(hf, err):
    """<name>.hits: lines NAME>=N or NAME=N (a reachability counter of sh_hits.h, upper case; # comments).
    Returns "" when vsh_host --hits printed every counter as asserted, else what is wrong: a probe that
    is identical to bash but never ran the feature it names proves nothing."""
    seen = {}
    for l in err.decode("latin-1").splitlines():
        m = re.match(r"^hits (\w+) (\d+)$", l)
        if m:
            seen[m.group(1)] = int(m.group(2))
    bad = []
    for l in hf.read_text().splitlines():
        l = l.split("#")[0].strip()
        if not l:
            continue
        m = re.match(r"^(\w+)(>=|=)(\d+)$", l)
        if not m:
            bad.append("bad hits line: " + l)
            continue
        name, op, n = m.group(1), m.group(2), int(m.group(3))
        v = seen.get(name)
        if v is None or (v < n if op == ">=" else v != n):
            bad.append("hits %s: want %s%d, got %s" % (name, op, n, v))
    return "\n".join(bad) + "\n" if bad else ""


def expected_text(out, st):
    return out + (b"[exit %d]\n" % st if isinstance(st, int) else b"[timeout]\n")


def check_probe(pid, p):
    """(identical, diff text)."""
    # the real path: bash keeps $PWD as typed, vsh asks the OS; a symlinked /var would differ
    base = pathlib.Path(tempfile.mkdtemp(prefix="bashdiff.")).resolve()
    try:
        eo, es, _ = run_one([ORACLE], p, base)
        want = expected_text(eo, es)
        exp = OUT / "expected" / (pid + ".txt")
        exp.parent.mkdir(parents=True, exist_ok=True)
        exp.write_bytes(want)
        hf = p.with_suffix(".hits")
        go, gs, gerr = run_one([str(VSH)], p, base, hits=hf.exists())
        got = expected_text(go, gs)
        bad = hits_check(hf, gerr) if hf.exists() else ""
    finally:
        shutil.rmtree(base, ignore_errors=True)
    dif = OUT / "diff" / (pid + ".diff")
    dif.parent.mkdir(parents=True, exist_ok=True)
    if want == got and bad:
        dif.write_text(bad)
        return False, bad
    if want == got:
        dif.unlink(missing_ok=True)
        return True, ""
    d = "\n".join(difflib.unified_diff(want.decode("latin-1").splitlines(), got.decode("latin-1").splitlines(),
                                       "bash", "vsh_host", lineterm=""))
    dif.write_text(d + "\n")
    return False, d


def read_verdict(area):
    f = OUT / (area + ".verdict")
    if not f.exists():
        return None, {}
    lines = f.read_text().splitlines()
    head = lines[0].split() if lines else []
    res = {}
    for l in lines[1:]:
        f3 = l.split("\t")
        if len(f3) == 3:
            res[f3[0]] = (f3[1], f3[2])
    return (head[1] if len(head) > 1 else None), res


def write_verdict(area, fp, res):
    OUT.mkdir(parents=True, exist_ok=True)
    status = "PASS" if res and all(v[0] == "PASS" for v in res.values()) else "FAIL"
    (OUT / (area + ".verdict")).write_text(
        "%s %s\n%s" % (status, fp, "".join("%s\t%s\t%s\n" % (n, v[0], v[1]) for n, v in sorted(res.items()))))


def leak_gate(only):
    """V44: no ratchet probe leaves heap blocks behind; V59: nor temp files in T:. Returns the exit status."""
    if not VSH_LEAK.exists():
        print("[ERROR] %s missing: make build/vsh_host_leak" % VSH_LEAK)
        return 2
    allp = probes()
    div, rat, errs = lists(allp)
    ids = [pid for pid in sorted(rat) if not only or pid == only or pid.split("/")[1] == only]
    bad = []
    with tempfile.TemporaryDirectory(prefix="bashdiff-leak-") as td:
        def one(i_pid):
            i, pid = i_pid
            base = pathlib.Path(td) / str(i)
            base.mkdir()
            out, st, err = run_one([str(VSH_LEAK)], allp[pid], base, leak=True)
            m = re.findall(rb"leak blocks=(-?\d+)", err)
            # V59: here-document, $( ) and process substitution temp files (T: is $TMPDIR here) are removed
            left = sorted(x for x in os.listdir(base / "tmp") if x.startswith("vsh-"))
            shutil.rmtree(base, ignore_errors=True)
            return pid, (int(m[-1]) if m else None), st, left
        with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
            for pid, n, st, left in ex.map(one, enumerate(ids)):
                if st == "timeout":
                    continue
                if left:
                    bad.append("%s leaves temp files: %s" % (pid, " ".join(left)))
                if n is None:
                    bad.append("%s printed no leak tally" % pid)
                elif n > LEAK_BASE:
                    bad.append("%s leaks %d heap blocks (baseline %d)" % (pid, n - LEAK_BASE, LEAK_BASE))
    for b in bad:
        print("[FAIL] leak: " + b)
    print("[INFO] leak gate: %d ratchet probes, %d leaking or leaving temp files" % (len(ids), len(bad)))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--only", metavar="AREA", help="one area")
    ap.add_argument("--probe", metavar="NAME", help="one probe (name or area/name), rerun")
    ap.add_argument("--failed", action="store_true", help="the probes whose verdict is FAIL")
    ap.add_argument("--force", action="store_true", help="rerun passed probes too")
    ap.add_argument("--gate", action="store_true", help="ratchet probes only, plus the list checks")
    ap.add_argument("--leak", action="store_true", help="V44 leak gate over the ratchet probes")
    ap.add_argument("-v", "--verbose", action="store_true", help="name every failing probe")
    a = ap.parse_args()
    if a.leak:
        return leak_gate(a.probe)
    allp = probes()
    div, rat, errs = lists(allp)
    if a.gate:
        for e in errs:
            print("[FAIL] " + e)
    try:
        ver = oracle_version()
    except Skip:
        if a.gate:
            print("[INFO] no bash 5 (%s): bashdiff gate skipped" % ORACLE)
            return 1 if errs else 0
        print("[ERROR] no oracle at %s (BASH_ORACLE=...): brew install bash" % ORACLE)
        return 2
    if not VSH.exists():
        print("[ERROR] %s missing: make build/vsh_host" % VSH)
        return 2
    fp = hashlib.sha256(VSH.read_bytes() + ver.encode()).hexdigest()[:16]
    areas = sorted({pid.split("/")[0] for pid in allp})
    if a.only:
        if a.only not in areas:
            print("[ERROR] no area %s (have: %s)" % (a.only, " ".join(areas)))
            return 2
        areas = [a.only]
    old = {ar: read_verdict(ar) for ar in areas}
    todo = []
    for pid, p in allp.items():
        ar = pid.split("/")[0]
        if ar not in areas:
            continue
        if a.gate and pid not in rat:
            continue
        ofp, res = old[ar]
        h = probe_hash(p)
        prior = res.get(pid)
        if a.probe:
            if a.probe not in (pid, pid.split("/")[1]):
                continue
        elif a.failed:
            if not prior or prior[0] != "FAIL":
                continue
        elif not a.force and prior and prior[0] == "PASS" and prior[1] == h and ofp == fp:
            continue
        todo.append((pid, p, h))
    if a.probe and not todo:
        print("[ERROR] no probe %s" % a.probe)
        return 2
    results = {ar: (dict(old[ar][1]) if old[ar][0] == fp else {}) for ar in areas}
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
        outs = list(ex.map(lambda t: check_probe(t[0], t[1]), todo))
    for (pid, p, h), (ok, _d) in zip(todo, outs):
        results[pid.split("/")[0]][pid] = ("PASS" if ok else "FAIL", h)
    for ar in areas:
        results[ar] = {k: v for k, v in results[ar].items() if k in allp}
        if todo or not (OUT / (ar + ".verdict")).exists():
            write_verdict(ar, fp, results[ar])
    # summary
    bad_ratchet, unclassified, stale = [], [], []
    tot_ok = tot = 0
    for ar in areas:
        res = results[ar]
        ids = [pid for pid in allp if pid.startswith(ar + "/")]
        ok = sum(1 for pid in ids if res.get(pid, ("", ""))[0] == "PASS")
        tot_ok, tot = tot_ok + ok, tot + len(ids)
        for pid in ids:
            v = res.get(pid, ("", ""))[0]
            if v == "FAIL" and pid in rat:
                bad_ratchet.append(pid)
            elif v == "FAIL" and pid not in div:
                unclassified.append(pid)
            elif v == "PASS" and pid in div and div[pid][0].startswith("later:"):
                stale.append(pid)
        if not a.gate:
            print("%-12s %3d of %3d identical to bash %s" % (ar + ":", ok, len(ids), ver.split("(")[0].strip()))
    ran = "ran %d probe(s)" % len(todo)
    if not a.gate:
        print("total: %d of %d identical (%s)" % (tot_ok, tot, ran))
    else:
        print("[INFO] bashdiff gate: %s, %d ratchet probes, %d failing" % (ran, len(rat), len(bad_ratchet)))
    for pid in bad_ratchet:
        print("[FAIL] ratchet probe differs from bash: %s (diff: build/bashdiff/diff/%s.diff)" % (pid, pid))
    if not a.gate:
        shown = unclassified if a.verbose or len(unclassified) <= 12 else unclassified[:12]
        for pid in shown:
            print("[FAIL] unclassified: %s" % pid)
        if len(shown) < len(unclassified):
            print("[FAIL] ... and %d more unclassified (-v lists them all)" % (len(unclassified) - len(shown)))
        for pid in stale[:12]:
            print("[INFO] identical now, move to ratchet.txt: %s" % pid)
        if len(stale) > 12:
            print("[INFO] ... and %d more identical now" % (len(stale) - 12))
        if not a.only and not a.probe and not a.failed:
            for e in errs:
                if "is in neither" not in e:
                    print("[FAIL] " + e)
    bad = bool(bad_ratchet) or (a.gate and bool(errs)) or (not a.gate and (bool(unclassified) or bool(errs)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
