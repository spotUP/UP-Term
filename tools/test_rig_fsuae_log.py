"""tools/rig/paths.py cap_fsuae_log and rig.py's logs_dir: FS-UAE's log lives in the rig's own dir and is capped.

2026-10-17: every rig's FS-UAE wrote ~/Documents/FS-UAE/Cache/Logs/fs-uae.log.txt, one shared, uncapped file;
it reached 7.9 GB and filled the Mac's disk. No FS-UAE is run here: the log dir per rig, the config line in
rig.py's source, and the cap on a fake oversized log (sparse, so it costs no disk) are checked."""
import os
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import paths  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


# the log dir is inside each rig's own build dir
for n, name in ((1, "rig"), (2, "rig2"), (3, "rig3")):
    out = subprocess.run([sys.executable, "-I", "-c",
                          "import sys; sys.path.insert(0, %r); import paths; print(paths.FSUAE_LOG_DIR); print(paths.RIG)"
                          % str(HERE / "rig")], capture_output=True, text=True,
                         env=dict(os.environ, UPTERM_RIG=str(n))).stdout.split("\n")
    check(out[0] == out[1] + "/logs" and out[1].endswith("/build/" + name), "rig %d: log dir %s inside %s" % (n, out[0], out[1]))

# rig.py writes it into the generated config, and sets no verbose log option
src = (HERE / "rig/rig.py").read_text()
check('"logs_dir = %s" % paths.FSUAE_LOG_DIR' in src, "rig.py writes logs_dir = the rig's log dir into the .fs-uae config")
check("log_bsdsocket" not in src and "log_flush" not in src and "log_input" not in src, "no verbose FS-UAE log option in rig.py")
check("cap_fsuae_log" in src and "cap_fsuae_log" in (HERE / "rig/ami.py").read_text(),
      "the cap runs at rig start and in ami.req")

# the cap: a fake log over the limit is cut, its tail kept; one under the limit is left alone
with tempfile.TemporaryDirectory() as d:
    d = pathlib.Path(d)
    big, small = d / "fs-uae.log.txt", d / "small.log.txt"
    with open(big, "wb") as f:
        f.write(b"A" * 2000 + b"B" * 100)
    small.write_bytes(b"x" * 10)
    check(paths.cap_fsuae_log(d, limit=0, tail=100) == ["fs-uae.log.txt", "small.log.txt"], "limit 0 cuts every log")
    check(big.stat().st_size == 0, "the oversized log is truncated in place")
    check((d / "fs-uae.log.txt.tail").read_bytes() == b"B" * 100, "its last bytes are kept in .tail")
    big.write_bytes(b"y" * 5000)
    check(paths.cap_fsuae_log(d, limit=1 << 20) == [] and big.stat().st_size == 5000, "a log under the limit is left alone")
    # a real-size hole: 200 MB sparse file with a text end; cut at the production limit, no disk used
    with open(big, "wb") as f:
        f.seek(200 * 1024 * 1024)
        f.write(b"END\n")
    check(big.stat().st_blocks * 512 < paths.FSUAE_LOG_MAX, "fixture is sparse")
    check(paths.cap_fsuae_log(d) == [], "allocated size, not apparent size, decides")
    big.write_bytes(os.urandom(1) * 0 + b"z" * (paths.FSUAE_LOG_MAX + 4096 * 4))
    check(paths.cap_fsuae_log(d) == ["fs-uae.log.txt"] and big.stat().st_size == 0, "a log past FSUAE_LOG_MAX is cut")
    check(len((d / "fs-uae.log.txt.tail").read_bytes()) == paths.FSUAE_LOG_TAIL, "the kept tail is FSUAE_LOG_TAIL long")
    check(paths.cap_fsuae_log(d / "missing") == [], "a missing log dir is not an error")

# the watcher: rig start spawns "rig.py logwatch", which caps the logs and serial.log while the rig runs
sys.path.insert(0, str(HERE / "rig"))
import rig  # noqa: E402
with tempfile.TemporaryDirectory() as d:
    d = pathlib.Path(d)
    saved = (paths.FSUAE_LOG_DIR, paths.FSUAE_LOG_MAX, paths.FSUAE, rig.SERIAL, rig.LOGWATCH_EVERY, rig.mine,
             rig.setup_config_only, rig.status, rig.boot_assigns_missing, rig.guard_shell_startup, rig.M060,
             rig.subprocess.Popen, rig.RIG, rig.time.sleep)
    paths.FSUAE_LOG_DIR, paths.FSUAE_LOG_MAX = d / "logs", 1000
    rig.SERIAL, rig.LOGWATCH_EVERY, rig.RIG = d / "serial.log", 0, d
    (d / "logs").mkdir()
    (d / "logs" / "fs-uae.log.txt").write_bytes(b"L" * 100000)
    rig.SERIAL.write_bytes(b"S" * 100000)
    ticks = iter([["fs-uae running"], ["fs-uae running"], []])
    rig.mine = lambda: next(ticks)
    rig.logwatch()
    check((d / "logs" / "fs-uae.log.txt").stat().st_size == 0, "logwatch caps the FS-UAE log while the rig runs")
    check(rig.SERIAL.stat().st_size == 0 and (d / "serial.log.tail").exists(), "logwatch caps serial.log too")
    spawned = []
    rig.subprocess.Popen = lambda cmd, **kw: spawned.append(cmd)
    rig.mine, rig.setup_config_only, rig.status = (lambda: []), (lambda: None), (lambda quiet=False: True)
    rig.boot_assigns_missing, rig.guard_shell_startup, rig.M060 = (lambda: []), (lambda: None), False
    rig.time.sleep = lambda s: None
    paths.FSUAE = pathlib.Path("/usr/bin/true")
    rig.start()
    check(sum(1 for c in spawned if c[-1] == "logwatch") == 1, "rig start spawns exactly one logwatch (spawned: %r)" % spawned)
    (paths.FSUAE_LOG_DIR, paths.FSUAE_LOG_MAX, paths.FSUAE, rig.SERIAL, rig.LOGWATCH_EVERY, rig.mine,
     rig.setup_config_only, rig.status, rig.boot_assigns_missing, rig.guard_shell_startup, rig.M060,
     rig.subprocess.Popen, rig.RIG, rig.time.sleep) = saved

print("rig_fsuae_log:", "FAIL (%d)" % fails if fails else "ok")
sys.exit(1 if fails else 0)
