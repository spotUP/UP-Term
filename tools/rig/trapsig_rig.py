#!/usr/bin/env python3
"""trapsig_rig.py -- V78 on the rig: which signals reach a vsh script's traps on the Amiga.

A vsh (VSH=<binary>, default build/amiga/vsh, copied to VTC:trapsig/vsh) runs, started with Run, a script
that sets traps on HUP INT QUIT TERM USR1 USR2 WINCH TSTP (each appends its name to RAM:trapsig.log) and
loops for up to 120 s. The rig then sends, one at a time, `kill -s SIG <pid>` from a second vsh for INT,
QUIT, TERM, HUP, USR1, and the AmigaDOS `Break <process> D` and `F`, and records which trap ran.

Must hold: INT runs the INT trap (Ctrl-C) and QUIT the QUIT trap (break bit E, which vsh's kill sends for
QUIT: before 2026-10-08 vsh polled Ctrl-C only and QUIT never arrived), and `Break <process> D` the TERM
trap (owner 2026-10-29: the AmigaDOS Ctrl-D break is TERM for vsh). The rest is printed as found: TERM and
HUP reach a native task as Ctrl-C (os_signal's fallback when ixkill cannot signal it), USR1 cannot be sent
to a native task, F maps to no signal (dist/README.txt, tests/bash/divergences.txt).

  UPTERM_RIG=2 python3 tools/rig/trapsig_rig.py

`trapsig_rig.py kill` runs only the two kill -s TERM checks of a native busy loop (VSH-KILL-TERM-NATIVE-HANG):
A) the target runs at priority 1 (wasabid's, which starves the priority-0 tasks ixkill's start needs): the sender
   must come back within 5 s of a Ctrl-C sent to it 3 s in (before the fix it stayed over 90 s, deaf to it);
B) the target runs at priority 0: the sender must come back within 10 s, and the target must end with status 143 (TERM reaches a native vsh as Ctrl-D, the fallback of os_signal; before s34 it was Ctrl-C and 130).

The rig must be up."""
import os, re, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import userland_rig as U

SCRIPT = """\
echo $$ >RAM:trapsig.pid
: >RAM:trapsig.log
for s in HUP INT QUIT TERM USR1 USR2 WINCH TSTP; do trap "echo $s >>RAM:trapsig.log" $s; done
trap 'echo EXIT >>RAM:trapsig.log' EXIT
while [ $SECONDS -lt 120 ]; do [ -e RAM:trapsig.stop ] && exit 0; done
"""
KILL_SCRIPT = """\
echo $$ >RAM:kt.pid
while [ $SECONDS -lt 100 ]; do [ -e RAM:kt.stop ] && exit 0; done
"""
# B runs the target from an AmigaDOS script: ChangeTaskPri there sets the priority of the process that runs the
# target (in a vsh script the command runs in another process), and its status is kept in RAM:kt.rc
KILL_RUN = """\
FailAt 9999
ChangeTaskPri 0
VTC:trapsig/vsh VTC:trapsig/kt.sh
Echo "$RC" >RAM:kt.rc
"""
# C/D: TERM to a target priority 1 (ixkill starved, os_signal falls back to a break). C is a vsh script with a
# TERM trap: Ctrl-D, so the trap runs (C); a script without a trap exits 143 (C2).
TERM_SCRIPT = """\
echo $$ >RAM:kt.pid
trap 'echo TERM >>RAM:kt.log' TERM
while [ $SECONDS -lt 100 ]; do [ -e RAM:kt.stop ] && exit 0; done
"""
TERM_PLAIN = """\
echo $$ >RAM:kt.pid
while [ $SECONDS -lt 100 ]; do [ -e RAM:kt.stop ] && exit 0; done
"""
TERM_RUN = """\
FailAt 9999
VTC:trapsig/vsh VTC:trapsig/%s
Echo "$RC" >RAM:kt.rc
"""
# D: a non-vsh native program (C:Wait) started by a vsh script as a job; the pid is the job's
WAIT_SCRIPT = """\
C:Wait 100 SEC &
echo $! >RAM:kt.pid
wait
"""
MUST = {"INT": "INT", "QUIT": "QUIT", "break-D": "TERM"}


def log_lines():
    rc, out = U.run("Type RAM:trapsig.log", 30)
    return [l.strip() for l in out.splitlines() if l.strip()]


def kill_check(name, pri0):
    """One of the kill checks; (ok, text). The sender runs in a thread so that A can break it from here."""
    import threading
    U.run("Delete RAM:kt.stop RAM:kt.pid RAM:kt.rc QUIET", 30)
    if pri0:
        U.run("Run >NIL: Execute VTC:trapsig/kt.run", 30)
    else:
        U.run("Run >NIL: VTC:trapsig/vsh VTC:trapsig/kt.sh", 30)
    time.sleep(3)
    pid = U.run("Type RAM:kt.pid", 30)[1].strip()
    res = {}

    def sender():
        t = time.time()
        try:
            res["r"] = U.run('VTC:trapsig/vsh -c "kill -s TERM %s"' % pid, 100)
        except BaseException as e:
            res["r"] = ("EXC", repr(e)[:100])
        res["secs"] = time.time() - t
    th = threading.Thread(target=sender)
    th.start()
    brk = None
    if not pri0:
        th.join(3)
        if th.is_alive():
            brk = time.time()
            import ami
            ami.req(0x09)
    th.join(110)
    secs = res.get("secs", 999.0)
    after = (time.time() - brk) if brk else None
    if pri0:
        good = secs < 10
        U.run("Echo x >RAM:kt.stop", 30)   # a target that survived
        time.sleep(3)
        rc = U.run("Type RAM:kt.rc", 30)[1].strip()
        good = good and rc == "143"
        text = "sender %.1f s, target status %s" % (secs, rc or "none")
    else:
        good = brk is not None and after is not None and secs < 3 + 5 + 1
        text = "sender %.1f s in all, Ctrl-C at 3 s" % secs
        U.run("Echo x >RAM:kt.stop", 30)
        time.sleep(3)
    print("%s %s -> %s %s" % ("ok  " if good else "FAIL", name, text, res.get("r", "")[:1]), flush=True)
    time.sleep(5)
    return good


def term_check(name, script, want_log, want_rc):
    """TERM at priority 1 with the Ctrl-C at 3 s to the sender (the fallback path); the target must end by itself."""
    import threading, ami
    U.run("Delete RAM:kt.stop RAM:kt.pid RAM:kt.rc RAM:kt.log QUIET", 30)
    U.run("Run >NIL: Execute VTC:trapsig/%s" % script, 30)
    time.sleep(3)
    pid = U.run("Type RAM:kt.pid", 30)[1].strip()
    res = {}

    def sender():
        t = time.time()
        res["r"] = U.run('VTC:trapsig/vsh -c "kill -s TERM %s"' % pid, 100)
        res["secs"] = time.time() - t
    th = threading.Thread(target=sender)
    th.start()
    th.join(3)
    if th.is_alive():
        ami.req(0x09)
    th.join(110)
    time.sleep(4)
    r = U.run("Type RAM:kt.log", 30)
    log = r[1].split() if r[0] == 0 else []
    r = U.run("Type RAM:kt.rc", 30)
    rc = r[1].strip() if r[0] == 0 else ""
    U.run("Echo x >RAM:kt.stop", 30)
    good = res.get("secs", 999) < 12 and (("TERM" in log) == want_log) and rc == want_rc
    print("%s %s -> sender %.1f s, trap log %s, target status %s (want log %s, status %s)" % (
        "ok  " if good else "FAIL", name, res.get("secs", 999), log, rc or "none", want_log, want_rc), flush=True)
    time.sleep(5)
    return good


def wait_check(name, sig, want_ended):
    """kill -s SIG at C:Wait (not an ixemul program: ixkill fails, os_signal falls back or does not). Proves the
    Ctrl-C branch: the same target ends after TERM (fallback) and survives USR1 (no fallback); RAM:kt.rc holds the
    job script's status only once the Wait ended, and the sender's status and output are printed."""
    U.run("Delete RAM:kt.stop RAM:kt.pid RAM:kt.rc RAM:kt.log QUIET", 30)
    U.run("Run >NIL: Execute VTC:trapsig/kt_wait.run", 30)
    time.sleep(3)
    pid = U.run("Type RAM:kt.pid", 30)[1].strip()
    t = time.time()
    r = U.run('VTC:trapsig/vsh -c "kill -s %s %s; echo kill-rc=$?"' % (sig, pid), 100)
    secs = time.time() - t
    time.sleep(6)
    rc = U.run("Type RAM:kt.rc", 30)
    ended = rc[0] == 0 and rc[1].strip() != ""
    U.run("Echo x >RAM:kt.stop", 30)
    good = ended == want_ended
    print("%s %s -> sender %.1f s, kill said %r, Wait ended %s (want %s)" % (
        "ok  " if good else "FAIL", name, secs, str(r[1]).strip()[:80], ended, want_ended), flush=True)
    return good


def kill_main():
    vsh = os.environ.get("VSH") or str(U.ROOT / "build/amiga/vsh")
    d = U.VTC / "trapsig"
    d.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(vsh, d / "vsh")
    (d / "kt.sh").write_text(KILL_SCRIPT, encoding="latin-1")
    (d / "kt.run").write_text(KILL_RUN, encoding="latin-1")
    ok = kill_check("A kill -s TERM, target priority 1, Ctrl-C to the sender", False)
    ok = kill_check("B kill -s TERM, target priority 0", True) and ok
    for f, c in (("kt_trap", TERM_SCRIPT), ("kt_plain", TERM_PLAIN)):
        (d / (f + ".sh")).write_text(c, encoding="latin-1")
        (d / (f + ".run")).write_text(TERM_RUN % (f + ".sh"), encoding="latin-1")
    (d / "kt_wait.sh").write_text(WAIT_SCRIPT, encoding="latin-1")
    (d / "kt_wait.run").write_text(TERM_RUN % "kt_wait.sh", encoding="latin-1")
    ok = term_check("C kill -s TERM, vsh script with a TERM trap, priority 1: trap runs (Ctrl-D)", "kt_trap.run", True, "") and ok
    ok = term_check("C2 kill -s TERM, vsh script without a trap, priority 1: exit 143 (Ctrl-D)", "kt_plain.run", False, "143") and ok
    ok = term_check("D kill -s TERM, native non-vsh program (C:Wait), priority 1: stops on Ctrl-C", "kt_wait.run", False, "0") and ok
    ok = wait_check("E kill -s TERM, C:Wait: the Ctrl-C fallback ends it", "TERM", True) and ok
    ok = wait_check("F kill -s USR1, C:Wait (control, no fallback): Wait survives", "USR1", False) and ok
    print("trapsig_rig kill: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    if sys.argv[1:] == ["kill"]:
        return kill_main()
    vsh = os.environ.get("VSH") or str(U.ROOT / "build/amiga/vsh")
    d = U.VTC / "trapsig"
    d.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(vsh, d / "vsh")
    (d / "trapsig.sh").write_text(SCRIPT, encoding="latin-1")
    U.run("Delete RAM:trapsig.stop RAM:trapsig.log RAM:trapsig.pid QUIET", 30)
    U.run("Run >NIL: VTC:trapsig/vsh VTC:trapsig/trapsig.sh", 30)
    time.sleep(3)
    pid = U.run("Type RAM:trapsig.pid", 30)[1].strip()
    status = U.run("Status", 30)[1]
    m = re.search(r"Process\s+(\d+): Loaded as command: VTC:trapsig/vsh", status)
    proc = m.group(1) if m else None
    seen, ok = len(log_lines()), True
    # The Break sends go before kill -s TERM/HUP/USR1: those go through ixkill and can hang the sender.
    kill = lambda s: (s, 'VTC:trapsig/vsh -c "kill -s %s %s"' % (s, pid))
    sends = [kill("INT"), kill("QUIT")] + ([("break-" + b, "Break %s %s" % (proc, b)) for b in ("D", "F")] if proc else [])
    sends += [kill(s) for s in ("TERM", "HUP", "USR1")]
    for name, cmd in sends:
        rc, out = U.run(cmd, 60)
        time.sleep(2)
        lines = log_lines()
        new = lines[seen:]
        seen = len(lines)
        got = " ".join(new) or "nothing"
        want = MUST.get(name)
        good = want is None or new == [want]
        ok = ok and good
        print("%s %-8s -> %s%s" % ("ok  " if good else "FAIL", name, got,
                                   "" if not out.strip() or rc == 0 else "  (sender: %s)" % out.strip().splitlines()[-1]),
              flush=True)
    U.run("Echo >RAM:trapsig.stop x", 30)
    time.sleep(3)
    print("trapsig_rig: %s (process %s, pid %s)" % ("PASS" if ok else "FAIL", proc, pid))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
