#!/usr/bin/env python3
"""rc161_rig.py -- vsh leaves pr_ExitData at 0 in every process it makes (VSH-EXT-RC161, self-checking).

vsh gave its runner, subshell and SystemTags child NP_ExitData = the job. A loaded ixemul program then saw
pr_ExitData != 0, ixnet's init_inet_daemon read it as an AmiTCP inetd DaemonMessage, failed to obtain the
socket and exit(161): `vsh -c "UP-Term:bin/true"` gave st=161. Through the real entry point (`vsh -c`):
  UP-Term:bin/true            -> st=0
  UP-Term:bin/mkdir RAM:x     -> st=0 and the directory exists
  a background job            -> Ctrl-C style kill ends it (job_child finds the child without NP_ExitData)
Each case is detached (Run) with a 20 s watchdog. FAIL on a vsh that still sets NP_ExitData (st=161).

  UPTERM_RIG=3 [VSH=<vsh binary>] python3 -I tools/rig/rc161_rig.py

The rig must be up with ixemul and UP-Term:bin/true installed; `make build/amiga/vsh` first."""
import os, pathlib, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
DIR = "RAM:rc161"

# (name, vsh -c line, the output it must give)
CASES = (
    ("true", 'UP-Term:bin/true; echo st=$?', "st=0\n"),
    ("mkdir", 'UP-Term:bin/mkdir RAM:rc161/made; echo st=$?; test -d RAM:rc161/made && echo exists', "st=0\nexists\n"),
    ("bg_kill", 'Wait 30 & kill %1; wait; echo ended', "ended\n"),
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


def run_case(name, line, want):
    base = "%s/%s" % (DIR, name)
    ex('Delete >NIL: "%s.out" "%s.done" QUIET' % (base, base))
    put_text("FailAt 1000\n%s/vsh -c \"%s\" >%s.out\nEcho >%s.done $RC\n" % (DIR, line, base, base), base + ".run")
    ex("Run >NIL: <NIL: Execute %s.run" % base)
    end = time.time() + 20
    while time.time() < end:
        if ami.read_file(base + ".done") is not None:
            out = (ami.read_file(base + ".out") or b"").decode("latin-1").replace("\r", "").replace("***Break\n", "")  # Wait's own break notice
            return out == want, out
        time.sleep(1)
    return False, "hung (no end after 20 s)"


def main():
    vsh = pathlib.Path(os.environ.get("VSH") or ROOT / "build/amiga/vsh")
    ex("Delete >NIL: %s ALL QUIET" % DIR)
    ex("MakeDir >NIL: %s" % DIR)
    ami.put(str(vsh), DIR + "/vsh")
    ok = True
    for name, line, want in CASES:
        good, out = run_case(name, line, want)
        ok = ok and good
        print("[%s] %s: %r" % ("OK" if good else "FAIL", name, out))
    print("rc161_rig: %s" % ("OK" if ok else "FAIL"))
    ex("Delete >NIL: %s ALL QUIET" % DIR)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
