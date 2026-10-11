#!/usr/bin/env python3
"""proc_rig.py -- ps and top on the rig (Unix tool ports 3.3; verdict PASS/FAIL per check).

Each run is a titled XCON: window started from a script (nothing typed but q in the last check):
  1. ps lists input.device (a known task) with its state and type
  2. an idle system: top -b says CPU busy below 15 %
  3. VTC:busyloop in a window of its own: top -b shows it at 85 % or more
  4. while top runs, the VBlank server list (VTC:intlist) has "top sampler"; after top ends by -n, by Ctrl-C
     (Break) and by q in its window, it has not
The rig must be up (rig.py start --max; UPTERM_RIG=2 for rig 2);
`make build/amiga/ps build/amiga/top build/amiga/busyloop build/amiga/intlist` first; they are copied to VTC:.

  proc_rig.py"""
import os, pathlib, re, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, ixpty_rig, paths, screen_rig, shstdin_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
ok_all = True


def check(good, what, detail=""):
    global ok_all
    ok_all &= bool(good)
    print("%s %s%s" % ("ok" if good else "FAIL", what, (": " + detail) if detail and not good else ""))


def run(cmd, t=30):
    return ixpty_rig.run(cmd, t)


def text(path):
    rc, out = run("Type %s" % path)
    return out.replace("\r", "") if rc == 0 else ""


def window(title, lines, wait_file=None, limit=60):
    """A titled window running the AmigaDOS lines (then EndCLI); waits for wait_file when given."""
    name = "proc-%s.script" % re.sub(r"[^A-Za-z0-9]", "_", title)
    (VTC / name).write_text("\n".join(lines + ["EndCLI"]) + "\n")
    run('Run >NIL: NewShell "XCON:0/20/640/200/%s/CLOSE" FROM VTC:%s' % (title, name), 10)
    if wait_file:
        end = time.time() + limit
        while time.time() < end and run("List >NIL: %s" % wait_file)[0] != 0:
            time.sleep(1)


def servers():
    return text("RAM:proc-int.txt") if run("VTC:intlist >RAM:proc-int.txt")[0] == 0 else ""


def busy_of(report):
    m = re.search(r"CPU busy\s*([0-9.]+) %", report)
    return float(m.group(1)) if m else None


def main():
    for f in ("ps", "top", "busyloop", "intlist"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    run("Delete RAM:proc-#? QUIET")

    window("proc ps", ["VTC:ps >RAM:proc-ps.txt", "Echo done >RAM:proc-ps.done"], "RAM:proc-ps.done")
    ps = text("RAM:proc-ps.txt")
    check(ps.startswith(" CLI  PRI  STATE") and re.search(r"waiting\s+task\s+input\.device", ps),
          "ps lists input.device as a waiting task", ps[:300])

    window("proc idle", ["VTC:top -b -d 3 -n 1 >RAM:proc-idle.txt", "Echo done >RAM:proc-idle.done"],
           "RAM:proc-idle.done")
    idle = text("RAM:proc-idle.txt")
    b = busy_of(idle)
    check(b is not None and b < 15.0, "an idle system: top -b says CPU busy %s %%" % b, idle[:300])

    window("proc busy", ["VTC:busyloop 14", "Echo done >RAM:proc-busyloop.done"])
    time.sleep(3)
    window("proc top", ["VTC:top -b -d 5 -n 1 >RAM:proc-busy.txt", "Echo done >RAM:proc-busy.done"],
           "RAM:proc-busy.done")
    busy = text("RAM:proc-busy.txt")
    row = next((l for l in busy.splitlines() if "busyloop" in l), "")
    pct = float(row.split()[0]) if row else None
    check(pct is not None and pct >= 85.0, "a busy loop: top shows it at %s %%" % pct, busy[:400])
    for _ in range(20):
        if run("List >NIL: RAM:proc-busyloop.done")[0] == 0:
            break
        time.sleep(1)
    check("top sampler" not in servers(), "no VBlank server left after top -n ended")

    # Ctrl-C: a long batch top, broken from outside
    window("proc break", ["VTC:top -b -d 2 -n 100 >RAM:proc-break.txt", "Echo done >RAM:proc-break.done"])
    time.sleep(6)
    during = servers()
    rc, out = run("Status COMMAND VTC:top")
    num = out.strip().split()[0] if out.strip() else ""
    check("top sampler" in during and num.isdigit(), "while top runs its VBlank server is there", during + out)
    if num.isdigit():
        run("Break %s C" % num)
    for _ in range(15):
        if run("List >NIL: RAM:proc-break.done")[0] == 0:
            break
        time.sleep(1)
    check(run("List >NIL: RAM:proc-break.done")[0] == 0 and "top sampler" not in servers(),
          "Ctrl-C ends top and removes its VBlank server")

    # q in an interactive top
    title = "proc interactive"
    window(title, ["VTC:top -d 1", "Echo done >RAM:proc-q.done"])
    time.sleep(5)
    if shstdin_rig.focus(title):
        ami.req(0x08, bytes([4]) + b"q")
    for _ in range(10):
        if run("List >NIL: RAM:proc-q.done")[0] == 0:
            break
        time.sleep(1)
    check(run("List >NIL: RAM:proc-q.done")[0] == 0 and "top sampler" not in servers(),
          "q ends an interactive top and removes its VBlank server")
    print("PASS" if ok_all else "FAIL")
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())
