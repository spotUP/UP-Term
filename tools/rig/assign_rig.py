#!/usr/bin/env python3
"""assign_rig.py -- UPTERM-VOLUME-REQUESTER: with UP-Term: not assigned,
nothing of UP-Term asks for a volume, and the assign is made from
ENVARC:up-term/Dir by whatever reads UP-Term: first (config/upassign.h).

Needs the kit built (make dist) and the rig free (tools/rig/rig.py). A fresh
boot, then Install, then four cases, each in its own XCON: window whose
script writes its results to RAM: (text, no screenshots); UITREE is polled
for a "System Request" window the whole time:

  install       install_rig.py's default pass (Install, Uninstall, byte-for-byte checks)
  noassign      no assign and no Dir file (not installed / removed): a vsh window with ls, cd,
                type, a tmux and a wide character -> no requester, every command answers
  assign        the assign there (the normal boot): the same window finds /UP-Term/bin/ls
  lazy-vsh      assign lost, Dir kept: vsh's startup makes the assign (the block of
                S:User-Startup is not what saves this boot)
  lazy-handler  assign lost, Dir kept, a window of native commands only: the handler's
                glyph worker makes the assign when it reads UP-Term:unifont/

Resumable: every case writes build/rig/assign/<case>.json (pass/fail, the
fingerprint of the kit files); a case that passed with the same fingerprint is
skipped.
  assign_rig.py                 the cases not yet passed
  assign_rig.py ONLY lazy-vsh   one case (re-run even when it passed)
  assign_rig.py FAILED          only the cases that failed last time
  assign_rig.py FORCE           all, from zero
Not run by `make test`: it needs the emulator."""
import hashlib, json, os, pathlib, struct, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import install_rig as ir

ROOT = ir.ROOT
VTC = ir.VTC
OUT = ROOT / "build/rig/assign"
DRAWER = "SYS:UP-Term"
CASES = ["install", "noassign", "assign", "lazy-vsh", "lazy-handler"]
WIDE = "\u4e2d"  # U+4E2D, a glyph of Unifont's page 0x4E

CASE_SCRIPTS = {
    "noassign": """Delete >NIL: RAM:ar.#? QUIET
VTC:vsh -c "echo hi >RAM:ar.hi"
VTC:vsh -c "ls /UP-Term/bin >RAM:ar.ls 2>&1"
VTC:vsh -c "cd /UP-Term/bin >RAM:ar.cd 2>&1"
VTC:vsh -c "type ls >RAM:ar.type 2>&1"
If EXISTS C:tmux
  C:tmux new-session -d -s assignrig
  C:tmux kill-server
EndIf
Echo "%s"
Echo >RAM:ar.done ok
EndCLI >NIL:
""",
    "assign": """Delete >NIL: RAM:ar.#? QUIET
VTC:vsh -c "ls /UP-Term/bin >RAM:ar.ls 2>&1"
VTC:vsh -c "type ls >RAM:ar.type 2>&1"
Echo >RAM:ar.done ok
EndCLI >NIL:
""",
    "lazy-vsh": """Delete >NIL: RAM:ar.#? QUIET
VTC:vsh -c "ls /UP-Term/bin >RAM:ar.ls 2>&1"
Assign >RAM:ar.assign UP-Term: EXISTS
Echo >RAM:ar.done ok
EndCLI >NIL:
""",
    "lazy-handler": """Delete >NIL: RAM:ar.#? QUIET
Echo "%s"
Wait 3
Assign >RAM:ar.assign UP-Term: EXISTS
Echo >RAM:ar.done ok
EndCLI >NIL:
""",
}
WINDOW = 'XCON:0/20/640/300/UP-Term/CLOSE'


def rig(cmd):
    """The default rig, the machine install_rig.py is written for (its 3.1
    disk: ORIG_SIZE is that ixemul). conbench_rig.rig() was used here and
    booted creep's --stock --os32 machine: 2 MB chip RAM and no fast RAM
    (Python3 "not enough memory", nvim past 400 s, CON: switches failed)
    and the 3.2 tree, every install check on the wrong system (2026-10-07)."""
    subprocess.run([sys.executable, str(ROOT / "tools/rig/rig.py"), cmd], check=True, timeout=400)


def fingerprint():
    h = hashlib.sha256()
    for p in ("dist/install.dos", "dist/Uninstall", "build/amiga/vsh", "build/amiga/vtcon-handler",
              "build/amiga/UPConsole", "build/amiga/up-console.device"):
        f = ROOT / p
        h.update(f.read_bytes() if f.exists() else b"missing " + p.encode())
    return h.hexdigest()[:16]


def verdict(case):
    f = OUT / (case + ".json")
    return json.loads(f.read_text()) if f.exists() else None


def record(case, ok, detail):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (case + ".json")).write_text(json.dumps(
        {"case": case, "verdict": "pass" if ok else "fail", "fp": fingerprint(), "detail": detail, "time": time.time()}))
    print("[%s] %s %s" % ("PASS" if ok else "FAIL", case, ("" if ok else detail[-300:])))


def requester_seen():
    tree = ami.req(0x0D).decode("latin-1")
    return "System Request" in tree or "Volume Request" in tree


def window_case(name, text):
    """Open WINDOW running the script, poll for requesters until RAM:ar.done; the files' contents."""
    (VTC / "assigncase").write_text(text.replace("%s", WIDE), encoding="utf-8")
    ir.run("Delete >NIL: RAM:ar.#? QUIET")
    ir.run('Run >NIL: NewShell "%s" FROM VTC:assigncase' % WINDOW)
    seen_req, end = False, time.time() + 120
    while time.time() < end:
        if requester_seen():
            seen_req = True
            for win, gad in (("System Request", "Cancel"), ("Volume Request", "_Cancel")):
                try:
                    ami.main(["ui", "click", win, gad])  # unblock the case, the verdict stays
                except SystemExit:
                    pass  # that window is not there
        if ir.run("List >NIL: RAM:ar.done")[0] == 0:
            break
        time.sleep(2)
    time.sleep(1)
    got = {}
    for part in ("hi", "ls", "cd", "type", "assign", "done"):
        rc, out = ir.run("Type RAM:ar.%s" % part)
        got[part] = out if rc == 0 else None
    return seen_req, got


def case_install():
    import subprocess
    rc = subprocess.run([sys.executable, "-u", str(ROOT / "tools/rig/install_rig.py")], timeout=3600).returncode
    return rc == 0, "install_rig exit %d" % rc


def case_noassign():
    ir.run("Assign UP-Term:")
    ir.run("Delete >NIL: ENVARC:up-term/Dir QUIET")
    req, got = window_case("noassign", CASE_SCRIPTS["noassign"])
    ok = (not req and got["done"] and got["hi"] and "hi" in got["hi"]
          and ir.run("Assign >NIL: UP-Term: EXISTS")[0] != 0)
    return bool(ok), "requester=%s files=%r assign made=%s" % (
        req, got, ir.run("Assign >NIL: UP-Term: EXISTS")[0] == 0)


def case_assign():
    ir.run('Assign UP-Term: "%s"' % DRAWER)
    req, got = window_case("assign", CASE_SCRIPTS["assign"])
    ok = not req and got["done"] and got["ls"] and "sort" in got["ls"] and got["type"] and "UP-Term" in got["type"]
    return bool(ok), "requester=%s files=%r" % (req, got)


def lazy(case):
    ir.run("Assign UP-Term:")
    ir.run("MakeDir >NIL: ENVARC:up-term")
    ir.run('Echo >ENVARC:up-term/Dir "%s"' % DRAWER)
    req, got = window_case(case, CASE_SCRIPTS[case])
    made = ir.run("Assign >NIL: UP-Term: EXISTS")[0] == 0
    ok = not req and got["done"] and made
    if case == "lazy-vsh":
        ok = ok and got["ls"] and "sort" in got["ls"]
    return bool(ok), "requester=%s assign made=%s files=%r" % (req, made, got)


def main():
    fx = ir.Fixtures(('ENVARC:up-term/Dir',))
    try:
        return _main(fx)
    finally:
        fx.restore()   # a killed or failing run leaves nothing planted


def _main(fx):
    args = sys.argv[1:]
    only = args[args.index("ONLY") + 1] if "ONLY" in args else None
    todo = [only] if only else CASES
    fp = fingerprint()
    ran_boot = installed = False
    for case in todo:
        v = verdict(case)
        if not only and "FORCE" not in args and v and v["verdict"] == "pass" and v["fp"] == fp:
            print("[SKIP] %s passed with this kit" % case)
            continue
        if "FAILED" in args and not (v and v["verdict"] == "fail"):
            continue
        if not ran_boot:
            rig("stop")
            rig("start")
            ir.prepare(None)
            fx.take()   # the user's files and assigns, before any case plants its own
            ran_boot = True
        if case != "install" and not installed:
            rc, out = ir.run_long("runinstall")  # Install (the cases need the kit in the rig)
            installed = rc == 0
            if not installed:
                record(case, False, "Install failed: " + out[-200:])
                return 1
        if case == "install":
            installed = False  # install_rig ends with Uninstall
        try:
            if case == "install":
                ok, detail = case_install()
            elif case == "noassign":
                ok, detail = case_noassign()
            elif case == "assign":
                ok, detail = case_assign()
            else:
                ok, detail = lazy(case)
        except SystemExit as e:  # the agent or the rig is gone: not a verdict on the code
            print("[ABORT] %s: %s (no verdict written)" % (case, e))
            return 2
        record(case, ok, detail)
    bad = [c for c in todo if (verdict(c) or {}).get("verdict") == "fail"]
    print("assign_rig: %d of %d cases failed: %s" % (len(bad), len(todo), " ".join(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
