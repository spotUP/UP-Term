#!/usr/bin/env python3
"""repeat_rig.py -- one bash probe run again and again on the rig, for a failure that comes now and then
(agent/a051_cmdsub_file hung once in 4 runs: plan 2026-10-07-vsh-bash, V8).

Each pass runs the probe as userland_rig.py --bash does (VTC:vsh on the probe in a copy of
tests/bash/data, output compared with build/bashdiff/expected/AREA/NAME.txt), in the background with a
watchdog. Every pass's verdict goes to build/<rig>/userland/repeat/<label>.log as it ends, and the next
run goes on from the last recorded pass (--fresh starts again). A pass that does not end in --limit
seconds: VTC:taskdump (make build/amiga/taskdump) prints where every task waits into
repeat/<label>-hang-<pass>.txt, the rig is rebooted, and the run stops (--go-on: next pass).

  python3 tools/rig/repeat_rig.py --bash agent --case a051_cmdsub_file --passes 100
  python3 tools/rig/repeat_rig.py --bash agent --case a051_cmdsub_file --text 'echo x | cat' --label cat
                                  (--text: another probe text, run in the same data copy; its expected
                                   output is --expect, default: the case's)
  VSH=<binary>: another vsh than build/amiga/vsh.

The rig must be up; UPTERM_RIG selects which one."""
import argparse, os, pathlib, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import userland_rig as U

ROOT = U.ROOT
VTC = U.VTC
LEDGER = paths.RIG / "userland" / "repeat"


def stage(area, case, label, text):
    pkg = "repeat"
    for d in (VTC / "userland" / pkg, VTC / "out" / pkg, VTC / "userland/bash" / area):
        d.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(os.environ.get("VSH") or ROOT / "build/amiga/vsh", VTC / "vsh")
    shutil.copyfile(ROOT / "build/amiga/taskdump", VTC / "taskdump")
    src = (U.BASH_PROBES / area / (case + ".sh")).read_text(encoding="latin-1") if text is None else text + "\n"
    (VTC / "userland/bash" / area / (label + ".sh")).write_text(src, encoding="latin-1")
    work = VTC / "userland/bashwork" / ("%s_%s" % (area, label))
    shutil.rmtree(work, ignore_errors=True)
    shutil.copytree(U.BASH_DATA, work)
    (VTC / "userland" / pkg / (label + ".sh")).write_text(U.bash_wrapper(area, label).replace("bash-%s/" % area, "repeat/"),
                                                         encoding="latin-1")
    if U.run("Assign >NIL: TMP: EXISTS", 20)[0] != 0:
        U.run("Assign TMP: T:", 20)


def one_pass(label, limit):
    """'ended' or 'hang' for one run of the staged probe."""
    done = VTC / "out/repeat" / (label + ".done")
    out = VTC / "out/repeat" / (label + ".txt")
    done.unlink(missing_ok=True)
    out.unlink(missing_ok=True)
    (VTC / "userland/repeat" / (label + ".run")).write_text(
        "VTC:vsh VTC:userland/repeat/%s.sh >VTC:out/repeat/%s.txt\necho done >VTC:out/repeat/%s.done\n"
        % (label, label, label), encoding="latin-1")
    try:
        U.run("Run >NIL: VTC:vsh VTC:userland/repeat/%s.run" % label, 20)
    except OSError:
        return "agent"   # amiagent itself no longer answers
    end = time.time() + limit
    while time.time() < end:
        if done.exists():
            return "ended"
        time.sleep(0.3)
    return "hang"


def reboot():
    rig = [sys.executable, str(pathlib.Path(__file__).with_name("rig.py"))]
    subprocess.run(rig + ["stop"], capture_output=True, timeout=120)
    subprocess.run(rig + ["start", "--max"], capture_output=True, timeout=400)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--bash", metavar="AREA", required=True)
    ap.add_argument("--case", required=True)
    ap.add_argument("--passes", type=int, default=100)
    ap.add_argument("--limit", type=float, default=20, help="seconds before a pass counts as hung")
    ap.add_argument("--text", help="another probe text (in the case's data)")
    ap.add_argument("--expect", help="expected output for --text (default: the case's)")
    ap.add_argument("--label", help="the ledger's name (default: the case; needed with --text)")
    ap.add_argument("--fresh", action="store_true", help="forget the recorded passes")
    ap.add_argument("--go-on", action="store_true", help="after a hang: reboot and go on")
    a = ap.parse_args()
    label = a.label or a.case
    if a.text and not a.label:
        ap.error("--text needs --label")
    LEDGER.mkdir(parents=True, exist_ok=True)
    log = LEDGER / (label + ".log")
    if a.fresh:
        log.unlink(missing_ok=True)
    lines = log.read_text().splitlines() if log.exists() else []
    first = len(lines) + 1
    if first > a.passes:
        print("[INFO] %s: %d passes recorded already (--fresh to start again)" % (label, len(lines)))
    exp = (a.expect + "\n[exit 0]" if a.expect is not None else
           (U.BASH_EXPECTED / a.bash / (a.case + ".txt")).read_text(encoding="latin-1")).splitlines()
    stage(a.bash, a.case, label, a.text)
    for i in range(first, a.passes + 1):
        how = one_pass(label, a.limit)
        got = VTC / "out/repeat" / (label + ".txt")
        if how != "ended":
            try:
                dump = U.run("VTC:taskdump", 30)[1]
            except OSError:
                dump = "amiagent does not answer (%s)\n" % how
            for code in (0x0D,):   # UITREE: the windows open now (a DOS requester a process waits in)
                try:
                    dump += "\n" + U.ami.req(code, timeout=30).decode("latin-1")
                except OSError:
                    pass
            (LEDGER / ("%s-hang-%d.txt" % (label, i))).write_text(dump)
            verdict = "HANG" if how == "hang" else "HANG-AGENT"
        else:
            verdict = "PASS" if got.exists() and got.read_text(encoding="latin-1").splitlines() == exp else "FAIL"
        with log.open("a") as f:
            f.write("pass %d %s\n" % (i, verdict))
        if verdict != "PASS":
            print("[FAIL] %s pass %d: %s" % (label, i, verdict), flush=True)
            if verdict.startswith("HANG"):
                reboot()
                if not a.go_on:
                    break
                stage(a.bash, a.case, label, a.text)
    lines = log.read_text().splitlines()
    good = sum(l.endswith(" PASS") for l in lines)
    print("%s: %d of %d recorded passes PASS (%s)" % (label, good, len(lines), log))
    return 0 if good == len(lines) == a.passes else 1


if __name__ == "__main__":
    sys.exit(main())
