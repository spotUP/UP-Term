"""rig.py aga/aga256/rtg reboot the rig as the same machine.

2026-10-19: `rig.py aga` on a rig started with --a1200 rebooted it with no arguments, so the config came back as
the default 060 + 1 GB machine and a 68020 timing run measured the 060. A start now keeps its arguments in
build/rig<N>/start.args and the screen mode reboot starts with them. No emulator is run: ami, stop and the
start process are stand-ins, and the arguments the new start gets are checked.
"""
import os
import pathlib
import sys
import tempfile
import types

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
saved_argv = sys.argv
sys.argv = ["rig.py", "test"]
import rig  # noqa: E402
sys.argv = saved_argv

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


sent, started, stops = [], [], []
sys.modules["ami"] = types.SimpleNamespace(req=lambda code, payload, **k: sent.append(payload) or b"\0\0\0\0")
rig.stop = lambda: stops.append(1)
rig.subprocess = types.SimpleNamespace(run=lambda cmd, **k: started.append(cmd))


def reboot(argv):
    del sent[:], started[:], stops[:]
    sys.argv = argv
    try:
        rig.screenmode(argv[1])
    finally:
        sys.argv = saved_argv
    return started[0][started[0].index("start") + 1:] if started else None


with tempfile.TemporaryDirectory() as d:
    rig.START_ARGS = pathlib.Path(d) / "start.args"

    # no start recorded (a rig started before this change): the default machine, as before
    check(reboot(["rig.py", "aga"]) == [], "no start.args: the reboot starts with no arguments")

    rig.START_ARGS.write_text('["--a1200"]')
    check(reboot(["rig.py", "aga"]) == ["--a1200"], "an --a1200 rig stays --a1200 after `rig.py aga`")
    check(b"screenmode.aga16.prefs" in sent[0], "aga copies the 16-colour AGA prefs: %r" % sent)
    check(stops == [1], "the rig is stopped once before the start")
    check(started[0][1].endswith("rig.py") and started[0][0] == sys.executable,
          "the start is a new rig.py process (the machine flags are read when rig.py loads): %r" % started[0])

    rig.START_ARGS.write_text('["--060", "--kick", "/x/kick.rom"]')
    check(reboot(["rig.py", "rtg"]) == ["--060", "--kick", "/x/kick.rom"], "every start argument is kept, values too")
    check(b"screenmode.rtg.prefs" in sent[0], "rtg copies the RTG prefs")

    check(reboot(["rig.py", "aga256", "--max"]) == ["--max"], "arguments given to aga256 win over the saved ones")

    rig.START_ARGS.write_text("not json")
    check(reboot(["rig.py", "aga"]) == [], "a damaged start.args: no arguments, no crash")

# start() records its arguments before it writes the config
src = (HERE / "rig/rig.py").read_text()
body = src[src.index("def start("):]
check(body.index("START_ARGS.write_text(json.dumps(") < body.index("setup_config_only()"),
      "start() writes start.args before the config")

print("rig_screenmode: %s" % ("FAIL (%d)" % fails if fails else "ok"))
sys.exit(1 if fails else 0)
