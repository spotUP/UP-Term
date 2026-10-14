"""wasabi on a rig: `rig.py wasabi on|off|snoop|debug|guru|ps`. Call trace, debug output, Guru reports and the task
list for the rigs (inventory Need 4, Q3-15 W-D1).

wasabi (MIT, https://github.com/creep-ltx/wasabi, `make wasabi` pins it into build/wasabi) is a daemon on the Amiga
(wasabid) and a one-file Python client. wasabid patches ~30 dos/exec/icon/diskfont calls (SnoopDOS style) and
RawPutChar (KPrintF) and keeps the last system alert; the client streams those. Here the daemon runs in the rig
under a rig key and the streams go to files the host reads: build/rig<N>/wasabi/snoop.log and debug.log.

  rig.py wasabi on                 start wasabid again after `off` (rig.py start boots it already), wait for ping
  rig.py wasabi off                stop the daemon (the patches come off with it; ami.py has no daemon then)
  rig.py wasabi snoop start [--task PATTERN] | stop | log [N]    call trace; log = the last N lines (30) and a count
  rig.py wasabi debug start | stop | log [N]                      KPrintF / RawPutChar output
  rig.py wasabi guru               the last Guru (T:lastguru on the Amiga) and alert lines in the two logs
  rig.py wasabi ps [PATTERN]       tasks and processes with stack headroom

Ports: rig N listens on 1234 + N - 1 (TCP, and UDP for discovery), like paths.AGENT_PORT. FS-UAE's bsdsocket
passes the Amiga's sockets to the Mac's, so two rigs on one port would clash. The key is the rig key below, never
a real one. Coexistence with amiagent: see tools/rig/README.md (wasabi section).
"""
import os
import pathlib
import re
import shutil
import signal
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths

DRAWER = "wasabi"                       # VTC:wasabi, <rig>/vtc/wasabi
BASE_PORT = 1234                        # wasabid's own default; rig N: BASE_PORT + N - 1 (paths.wasabi_port)
KEY = paths.WASABI_KEY                  # the rig key. Not a secret.
STREAMS = ("snoop", "debug")
USAGE = ("usage: rig.py wasabi on | off | snoop start [--task PATTERN] | stop | log [N]"
         " | debug start | stop | log [N] | guru | ps [PATTERN]")


def port(rig_n=None):
    """The TCP/UDP port wasabid listens on in rig `rig_n` (this one by default)."""
    return paths.wasabi_port(rig_n)


_CLIENT = []


def load_client():
    """wasabi's own client file (build/wasabi/wasabi, no .py) imported as a module: Conn, do_run, read_ui,
    grab_pixels ... ami.py is a thin client of these; the protocol is not re-implemented anywhere in vtcon."""
    if not _CLIENT:
        import importlib.machinery
        import importlib.util
        path = str(build_dir() / "wasabi")
        loader = importlib.machinery.SourceFileLoader("wasabi_client", path)
        spec = importlib.util.spec_from_loader("wasabi_client", loader)
        mod = importlib.util.module_from_spec(spec)
        sys.modules["wasabi_client"] = mod
        loader.exec_module(mod)
        _CLIENT.append(mod)
    return _CLIENT[0]


def out_dir(rig=None):
    return pathlib.Path(rig or paths.RIG) / "wasabi"


def log_path(stream, rig=None):
    return out_dir(rig) / (stream + ".log")


def pid_path(stream, rig=None):
    return out_dir(rig) / (stream + ".pid")


def build_dir():
    """build/wasabi of this checkout, else of the main checkout (a worktree shares the main one)."""
    for d in (paths.VTCON / "build" / "wasabi", paths.RIG_HOME / "wasabi"):
        if (d / "wasabid").is_file() and (d / "wasabi").is_file():
            return d
    raise SystemExit("rig wasabi: not built: run `make wasabi` in %s" % paths.VTCON)


def parse_args(argv):
    """(command, options) of `rig.py wasabi ...` (argv after the word wasabi); SystemExit with the usage line for
    anything else. options: sub (start|stop|log), task, n (log lines), pattern (ps)."""
    if not argv or argv[0] not in ("on", "off", "snoop", "debug", "guru", "ps"):
        raise SystemExit(USAGE)
    cmd, rest = argv[0], list(argv[1:])
    opts = {"sub": "", "task": "", "n": 30, "pattern": ""}

    def bad(what):
        raise SystemExit("rig.py wasabi %s: %s\n%s" % (cmd, what, USAGE))

    if cmd in ("on", "off", "guru"):
        if rest:
            bad("unknown argument %s" % rest[0])
    elif cmd == "ps":
        if len(rest) > 1:
            bad("unknown argument %s" % rest[1])
        opts["pattern"] = rest[0] if rest else ""
    else:
        if not rest or rest[0] not in ("start", "stop", "log"):
            bad("start, stop or log expected")
        opts["sub"], rest = rest[0], rest[1:]
        if opts["sub"] == "start" and cmd == "snoop" and rest[:1] == ["--task"]:
            if len(rest) < 2:
                bad("--task needs a pattern")
            opts["task"], rest = rest[1], rest[2:]
        elif opts["sub"] == "log" and rest and rest[0].isdigit():
            opts["n"], rest = int(rest[0]), rest[1:]
        if rest:
            bad("unknown argument %s" % rest[0])
    return cmd, opts


def client_args(rig_n=None):
    """The client's connection flags: this rig's port and the rig key, the host always the Mac itself (FS-UAE's
    bsdsocket shares its sockets). Flags beat WASABI_KEY and the client's config file."""
    return ["--host", "127.0.0.1", "--port", str(port(rig_n)), "--key", KEY]


def client_cmd(*args, rig_n=None):
    return [sys.executable, "-I", str(build_dir() / "wasabi")] + client_args(rig_n) + list(args)


def daemon_cmd(rig_n=None):
    """The AmigaDOS command line that starts wasabid in the rig (detached; its name tells discovery which rig)."""
    n = paths.RIG_N if rig_n is None else rig_n
    return "C:Run >NIL: <NIL: VTC:%s/wasabid %d name rig%d" % (DRAWER, port(n), n)


_CALL = re.compile(r"^(?:\S+ \S+ \w+ \| )?(?P<task>.+?)\s{2,}(?P<call>\w+)\((?P<args>.*)\)\s*=\s*(?P<result>.+?)\s*$")


def parse_calls(text):
    """The snoop lines in a log as dicts {task, call, args, result}: `Background CLI  Lock("SYS:", read) = ok`.
    Lines of any other shape (the connect and heartbeat notes) are skipped."""
    rows = []
    for line in text.splitlines():
        m = _CALL.match(line)
        if m:
            rows.append(m.groupdict())
    return rows


def summary(rows, n=30):
    """The last n calls, one per line, and the count per call name."""
    if not rows:
        return "no calls"
    counts = {}
    for r in rows:
        counts[r["call"]] = counts.get(r["call"], 0) + 1
    top = ", ".join("%s %d" % (k, v) for k, v in sorted(counts.items(), key=lambda kv: -kv[1])[:8])
    lines = ["%s: %s(%s) = %s" % (r["task"], r["call"], r["args"], r["result"]) for r in rows[-n:]]
    return "%d calls (%s)\n%s" % (len(rows), top, "\n".join(lines))


def run(cmd, timeout=30):
    import ami
    b = ami.req(0x02, struct.pack(">H", timeout) + cmd.encode("latin-1"))
    return struct.unpack(">I", b[:4])[0], b[4:].decode("latin-1")


def drawer():
    return paths.RIG / "vtc" / DRAWER


def alive():
    r = subprocess.run(client_cmd("ping"), capture_output=True, text=True, timeout=20)
    return r.returncode == 0


def on():
    with paths.rig_lock():
        if alive():
            print("wasabid is already running on port %d" % port())
            return
        d = drawer()
        d.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build_dir() / "wasabid", d / "wasabid")
        out_dir().mkdir(parents=True, exist_ok=True)
        run("C:SetEnv wasabi.key %s" % KEY)
        run(daemon_cmd())
        for _ in range(15):
            time.sleep(1)
            if alive():
                print("wasabid on, port %d (rig %d); logs in %s" % (port(), paths.RIG_N, out_dir()))
                return
        raise SystemExit("rig wasabi: wasabid did not answer on 127.0.0.1:%d (rig up? bsdsocket? `make wasabi`)" % port())


def stop_stream(stream):
    p = pid_path(stream)
    if not p.is_file():
        return False
    try:
        os.kill(int(p.read_text()), signal.SIGTERM)
    except (OSError, ValueError):
        pass
    p.unlink()
    return True


def off():
    with paths.rig_lock():
        for s in STREAMS:
            stop_stream(s)
        subprocess.run(client_cmd("quit", "--yes"), capture_output=True, text=True, timeout=30)
        time.sleep(2)
        print("[ERROR] wasabid still answers" if alive() else "wasabid off")


def stream(name, opts):
    sub = opts["sub"]
    if sub == "start":
        if not alive():
            raise SystemExit("rig wasabi: wasabid is not running (wasabi on first)")
        stop_stream(name)
        out_dir().mkdir(parents=True, exist_ok=True)
        lp = log_path(name)
        lp.write_text("")
        extra = ["--task", opts["task"]] if opts["task"] else []
        p = subprocess.Popen(client_cmd(name, "--once", "--log", str(lp), *extra), stdout=subprocess.DEVNULL,
                             stderr=(out_dir() / (name + ".err")).open("w"), start_new_session=True)
        pid_path(name).write_text(str(p.pid))
        print("%s streaming to %s" % (name, lp))
    elif sub == "stop":
        print("%s stopped" % name if stop_stream(name) else "%s was not streaming" % name)
    else:
        lp = log_path(name)
        if not lp.is_file():
            raise SystemExit("rig wasabi: no %s (%s start first)" % (lp, name))
        text = lp.read_text(encoding="latin-1")
        if name == "snoop":
            print(summary(parse_calls(text), opts["n"]))
        else:
            lines = text.splitlines()
            print("\n".join(lines[-opts["n"]:]) if lines else "no debug output")


def guru():
    r = subprocess.run(client_cmd("get", "T:lastguru", "-"), capture_output=True, text=True, timeout=30)
    print(r.stdout.strip() if r.returncode == 0 and r.stdout.strip() else "no Guru recorded (T:lastguru absent)")
    for s in STREAMS:
        lp = log_path(s)
        if lp.is_file():
            for line in lp.read_text(encoding="latin-1").splitlines():
                if re.search(r"guru|alert", line, re.I):
                    print("%s.log: %s" % (s, line))


def ps(opts):
    r = subprocess.run(client_cmd("ps", *([opts["pattern"]] if opts["pattern"] else [])), capture_output=True,
                       text=True, timeout=30)
    sys.stdout.write(r.stdout)
    if r.returncode:
        raise SystemExit(r.stderr.strip() or "rig wasabi: ps failed (wasabi on first)")


def main(argv):
    cmd, opts = parse_args(argv)
    if cmd == "on":
        on()
    elif cmd == "off":
        off()
    elif cmd == "guru":
        guru()
    elif cmd == "ps":
        ps(opts)
    else:
        stream(cmd, opts)
