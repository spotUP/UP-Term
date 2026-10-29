#!/usr/bin/env python3
"""nwait_rig.py -- kill SIGINT ends an ixemul program blocked in a native exec Wait() (RJ.7).

curl under `upkg update` waits in bsdsocket WaitSelect (Curl_amiga_select) with a Ctrl-C break mask, not in
any of ixemul's own waits. ixemul's setrun signalled only those (SWAIT, SSLEEP, select), so upkg's
kill(curl, SIGINT) set the pending bit and nothing woke curl: it lingered until the server answered.

Runs on the ixemul.library the rig booted with (rig.py start copies IXEMUL=, default the build295 one, to
VTC:ixp6; a library swapped in later is not loaded while vsh or wasabi hold the old one open, so the script
stops when VTC:ixp6 differs from IXEMUL), starts tests/amiga/ixnwait
(catches SIGINT, then Wait()s on SIGBREAKF_CTRL_C and a bit no one sends), sends `VTC:ixkill -INT <pid>`
from a second ixemul program and reads RAM:ixnwait.out. Passes when the Wait returned within 5 s and the
SIGINT handler ran. A program still waiting is ended with `Break <process> C` so the rig is left clean.
The rig must be up; `make build/amiga/ixnwait build/amiga/ixkill` first.

  UPTERM_RIG=<n> [IXEMUL=<ixemul.library> IXNET=<ixnet.library>] python3 -I tools/rig/nwait_rig.py"""
import hashlib, os, pathlib, re, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
run = ixpty_rig.run


def read_ram(name):
    rc, out = run("Type RAM:%s" % name, 30)
    return out.strip() if rc == 0 else ""


def main():
    for p in ("ixnwait", "ixkill"):
        shutil.copyfile(ROOT / "build/amiga" / p, VTC / p)
    md5 = lambda f: hashlib.md5(pathlib.Path(f).read_bytes()).hexdigest()
    if md5(VTC / "ixp6/ixemul.library") != md5(ixpty_rig.IXEMUL):
        print("nwait_rig: the rig did not boot %s; restart it with the same IXEMUL= and IXNET=" % ixpty_rig.IXEMUL)
        return 2
    run("Delete RAM:ixnwait.pid RAM:ixnwait.out QUIET", 30)
    run("Run >NIL: VTC:ixnwait", 30)
    end = time.time() + 20
    while time.time() < end and not read_ram("ixnwait.pid"):
        time.sleep(1)
    pid = read_ram("ixnwait.pid")
    if not pid:
        print("nwait_rig: FAIL (ixnwait did not start; ixemul %s)" % ixpty_rig.IXEMUL)
        return 1
    rc, out = run("VTC:ixkill -INT %s" % pid, 30)
    end = time.time() + 5
    while time.time() < end and not read_ram("ixnwait.out"):
        time.sleep(1)
    got = read_ram("ixnwait.out")
    if not got:
        m = re.search(r"Process\s+(\d+): Loaded as command: VTC:ixnwait", run("Status", 30)[1])
        if m:
            run("Break %s C" % m.group(1), 30)
    m = re.match(r"woke \S+ ints (\d+)", got)
    ok = bool(m) and int(m.group(1)) > 0
    print("nwait_rig: %s (ixkill rc %d %r; ixnwait %r; ixemul %s)"
          % ("OK" if ok else "FAIL", rc, out.strip(), got or "still waiting after 5 s", ixpty_rig.IXEMUL))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
