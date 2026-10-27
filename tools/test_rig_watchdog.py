"""The idle watchdog (RIG-IDLE-WATCHDOG, 2026-10-27): rig.py watchdog_step decides, with an injected clock and a fake
stop function, whether an idle rig is stopped. Also the stamp/flag helpers in paths.py that ami.py writes."""
import os
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
saved = sys.argv
sys.argv = ["rig.py", "test"]
import rig  # noqa: E402
sys.argv = saved

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


check(hasattr(rig, "watchdog_step"), "rig.watchdog_step exists")
if hasattr(rig, "watchdog_step"):
    def run(now, rig_n=7, idle=60, last=0.0, long_active=False):
        calls = []
        r = rig.watchdog_step(now, rig_n, idle, last, long_active, lambda: calls.append(1))
        return r, len(calls)

    check(run(61 * 60) == (True, 1), "idle past N minutes stops once")
    check(run(60 * 60) == (True, 1), "idle exactly N minutes stops")
    check(run(59 * 60) == (False, 0), "59 minutes idle of 60 does not stop")
    check(run(61 * 60, last=61 * 60 - 2) == (False, 0), "a command 2 s ago does not stop")
    check(run(2 * 60, idle=1, last=2 * 60 - 3) == (False, 0), "quiet under 5 s blocks even past the limit")
    check(run(61 * 60, long_active=True) == (False, 0), "long-run flag blocks")
    check(run(61 * 60, rig_n=1) == (False, 0), "rig 1 is never stopped")
    check(run(10 ** 6, idle=0) == (False, 0), "idle-minutes 0 disables")
    check(run(10 ** 6, last=None) == (False, 0), "no stamp recorded: no stop")

with tempfile.TemporaryDirectory() as d:
    p = pathlib.Path(d)
    check(rig.paths.last_command(p) is None, "no stamp before a command")
    rig.paths.touch_command(p)
    check(rig.paths.last_command(p) is not None, "touch_command writes the stamp")
    check(not rig.paths.long_run_active(p), "no flag, no long run")
    with rig.paths.long_run(p):
        check(rig.paths.long_run_active(p), "long_run holds the flag inside the block")
    check(not rig.paths.long_run_active(p), "long_run clears the flag after the block")
    (p / "long-run").write_text("999999999\n")
    check(not rig.paths.long_run_active(p), "a dead holder's flag is stale")

print("FAILED %d" % fails if fails else "ok")
sys.exit(1 if fails else 0)
