"""tools/rig/wasabi.py: `rig.py wasabi on|off|snoop|debug|guru|ps` -- call trace, debug output, Guru, tasks on a rig.

Fed a real wasabid 0.2 snoop log (rig 3, 2026-10-14, `List SYS:`), the argument handling, the port per rig, the
log paths and the daemon command line are driven the way rig.py calls them, and `rig.py wasabi` is run as a
program for its usage line. The reachability (wasabid really runs in a rig and streams) is: `rig.py wasabi on`,
`snoop start`, `ami.py exec "List SYS:"`, `snoop stop`, `snoop log`, `ps`, `off` (README, "Debugging on a rig").
"""
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import wasabi  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


SAMPLE = """2026-10-14 19:28:46.902 client | snoop stream open to 127.0.0.1:1236 (wasabid 0.2), --once - Ctrl-C to stop
2026-10-14 19:28:48.974 snoop | List                 LoadSeg("List") = ok
2026-10-14 19:28:48.974 snoop | List                 Lock("SYS:", read) = ok
2026-10-14 19:28:48.996 snoop | Background CLI       Open("RAM:x", read) = fail (Error 205: Object not found)
2026-10-14 19:28:48.997 snoop |   ... 1 more identical
"""
rows = wasabi.parse_calls(SAMPLE)
check(len(rows) == 3, "three calls parse out of a log with a connect note and a fold line, got %d" % len(rows))
if len(rows) == 3:
    check((rows[1]["task"], rows[1]["call"], rows[1]["args"], rows[1]["result"]) ==
          ("List", "Lock", '"SYS:", read', "ok"), "Lock line: task, call, args, result: %r" % rows[1])
    check(rows[2]["result"].startswith("fail (Error 205"), "a failing call keeps its error: %r" % rows[2])
check("3 calls" in wasabi.summary(rows) and 'Lock("SYS:", read) = ok' in wasabi.summary(rows), "summary counts and lists calls")
check(wasabi.parse_calls("") == [] and wasabi.summary([]) == "no calls", "an empty log has no calls")

# one port per rig: base + N - 1, like paths.AGENT_PORT
check([wasabi.port(n) for n in (1, 2, 3)] == [1234, 1235, 1236], "ports of rigs 1, 2, 3 are 1234, 1235, 1236")
check(len({wasabi.port(n) for n in range(1, 6)}) == 5, "no two rigs share a port")
check(wasabi.port() == wasabi.BASE_PORT + wasabi.paths.RIG_N - 1, "this rig's port follows UPTERM_RIG")
a = wasabi.client_args(3)
check(a[a.index("--port") + 1] == "1236" and a[a.index("--key") + 1] == wasabi.KEY and a[a.index("--host") + 1] == "127.0.0.1",
      "client flags: host, this rig's port, the rig key")
check("1235 name rig2" in wasabi.daemon_cmd(2) and wasabi.daemon_cmd(2).startswith("C:Run >NIL: <NIL: "), "daemon command line: detached, port, name")

# log paths under the rig's own drawer
check(wasabi.log_path("snoop", "/x/build/rig3") == pathlib.Path("/x/build/rig3/wasabi/snoop.log"), "snoop log under <rig>/wasabi")
check(wasabi.log_path("debug", "/x/build/rig2").name == "debug.log", "debug log name")

# argument handling
P = wasabi.parse_args
check(P(["on"])[0] == "on" and P(["guru"])[0] == "guru", "on, guru")
check(P(["ps", "List"])[1]["pattern"] == "List" and P(["ps"])[1]["pattern"] == "", "ps pattern")
check(P(["snoop", "start", "--task", "List*"])[1]["task"] == "List*", "snoop start --task")
check(P(["snoop", "log", "5"])[1]["n"] == 5 and P(["debug", "log"])[1]["n"] == 30, "log N, default 30")
for bad in ([], ["start"], ["on", "x"], ["snoop"], ["snoop", "go"], ["debug", "start", "--task", "x"], ["snoop", "start", "--task"],
            ["ps", "a", "b"], ["snoop", "log", "5", "6"]):
    try:
        P(bad)
        check(False, "arguments %r are refused" % (bad,))
    except SystemExit as e:
        check("usage" in str(e), "arguments %r are refused with the usage line" % (bad,))

r = subprocess.run([sys.executable, "-I", str(HERE / "rig/rig.py"), "wasabi", "bogus"],
                   capture_output=True, text=True, env={k: v for k, v in os.environ.items() if k != "UPTERM_RIG"})
check(r.returncode != 0 and "usage: rig.py wasabi" in r.stderr, "rig.py wasabi bogus: usage, non-zero: %r" % r.stderr[-120:])

sys.exit(1 if fails else 0)
