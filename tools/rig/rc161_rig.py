#!/usr/bin/env python3
"""rc161_rig.py -- vsh leaves pr_ExitData at 0 in every process it makes (VSH-EXT-RC161, self-checking).

vsh gave its runner, subshell and SystemTags child NP_ExitData = the job. A loaded ixemul program then saw
pr_ExitData != 0, ixnet's init_inet_daemon read it as an AmiTCP inetd DaemonMessage, failed to obtain the
socket and exit(161): `vsh -c "UP-Term:bin/true"` gave st=161. Through the real entry point (`vsh -c`):
  UP-Term:bin/true            -> st=0
  UP-Term:bin/mkdir RAM:x     -> st=0 and the directory exists
  a background job            -> Ctrl-C style kill ends it (job_child finds the child without NP_ExitData)
The SystemTags branch of the runner (a Resident command or a script: a Shell process of its own, job_child finds it
by cli_StandardInput == the job's input) is reached by:
  Resident ixemul program     -> `rtrue; echo st=$?` st=0 (the rc161 case inside the child; rtrue = Resident of bin/true)
  background script `Wait 30` -> `kill %1` ends it within 12 s (kill right after the & finds no child yet: see the design file), and `wasabi ps` shows its "Background CLI" Shell
Each case is detached (Run) with a 20 s watchdog. FAIL on a vsh that still sets NP_ExitData (st=161).

  UPTERM_RIG=3 [VSH=<vsh binary>] python3 -I tools/rig/rc161_rig.py

The rig must be up with ixemul and UP-Term:bin/true installed; `make build/amiga/vsh` first."""
import os, pathlib, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
DIR = "RAM:rc161"

# (name, vsh -c line, the output it must give)
SCRIPT = DIR + "/w.script"
CASES = (
    ("true", 'UP-Term:bin/true; echo st=$?', "st=0\n"),
    ("mkdir", 'UP-Term:bin/mkdir RAM:rc161/made; echo st=$?; test -d RAM:rc161/made && echo exists', "st=0\nexists\n"),
    ("bg_kill", 'Wait 30 & kill %1; wait; echo ended', "ended\n", 20),
    # the SystemTags branch: setup commands run before, teardown after (AmigaDOS lines)
    ("resident_true", 'rtrue; echo st=$?', "st=0\n", 20, "Resident rtrue UP-Term:bin/true PURE", "Resident rtrue REMOVE"),
    ("script_kill", SCRIPT + ' & sleep 2; kill %1; wait; echo ended', "ended\n", 12),
)


def put_text(text, path):
    with tempfile.NamedTemporaryFile("w", delete=False) as t:
        t.write(text)
    try:
        ami.put(t.name, path)
    finally:
        os.unlink(t.name)


def ex(cmd, secs=20):
    return ami.req(0x02, (secs).to_bytes(2, "big") + cmd.encode("latin-1"))


def run_case(name, line, want, secs=20, setup=None, teardown=None):
    if setup:
        ex(setup)
    try:
        return run_case1(name, line, want, secs)
    finally:
        if teardown:
            ex(teardown)


def ps_evidence():
    """Start the script job, look at the task list while it waits, then end it: the SystemTags child must be there."""
    wasabi = os.environ.get("WASABI") or str(ROOT / "build/wasabi/wasabi")
    n = int(os.environ.get("UPTERM_RIG", "1") or 1)
    ex("Run >NIL: <NIL: %s/vsh -c \"%s & wait\"" % (DIR, SCRIPT))
    time.sleep(4)
    ps = subprocess.run([sys.executable, wasabi, "--host", "127.0.0.1", "--port", str(1233 + n), "--key", "rigwasabi", "ps"],
                        capture_output=True, text=True).stdout
    hit = [l[:100] for l in ps.splitlines() if "vsh job" in l or "Background CLI (Wait)" in l]
    return hit


def run_case1(name, line, want, secs):
    base = "%s/%s" % (DIR, name)
    ex('Delete >NIL: "%s.out" "%s.done" QUIET' % (base, base))
    put_text("FailAt 1000\n%s/vsh -c \"%s\" >%s.out\nEcho >%s.done $RC\n" % (DIR, line, base, base), base + ".run")
    ex("Run >NIL: <NIL: Execute %s.run" % base)
    end = time.time() + secs
    while time.time() < end:
        if ami.read_file(base + ".done") is not None:
            out = (ami.read_file(base + ".out") or b"").decode("latin-1").replace("\r", "").replace("***Break\n", "")  # Wait's own break notice
            return out == want, out
        time.sleep(1)
    return False, "hung (no end after %d s)" % secs


def main():
    vsh = pathlib.Path(os.environ.get("VSH") or ROOT / "build/amiga/vsh")
    ex("Delete >NIL: %s ALL QUIET" % DIR)
    ex("MakeDir >NIL: %s" % DIR)
    ami.put(str(vsh), DIR + "/vsh")
    put_text("Wait 30\n", SCRIPT)
    ex("Protect %s +s" % SCRIPT)
    ev = ps_evidence()
    print("ps while the script job waits: %r" % ev)
    ok = any("Background CLI (Wait)" in l for l in ev) and any("vsh job" in l for l in ev)
    print("[%s] systemtags child present (Background CLI under vsh job)" % ("OK" if ok else "FAIL"))
    time.sleep(2)
    for name, line, want, *rest in CASES:
        good, out = run_case(name, line, want, *rest)
        ok = ok and good
        print("[%s] %s: %r" % ("OK" if good else "FAIL", name, out))
    print("rc161_rig: %s" % ("OK" if ok else "FAIL"))
    ex("Delete >NIL: %s ALL QUIET" % DIR)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
