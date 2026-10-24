#!/usr/bin/env python3
"""wbrun_rig.py -- tests/amiga/wbrun launches an icon as a Workbench double-click does, and its window opens.

Rigs and the A1200 run Directory Opus in Workbench's place: workbench.library's OpenWorkbenchObjectA()
fails there (IoErr 103, which C:WBRun prints as "not enough memory"), so wbrun builds the WBStartup
itself (thoughts/shared/research/2026-10-17_wb-icon-launch.md). Two icons, each checked through
wasabi's window list (text, no screenshot):

  - SYS:Utilities/Clock, a tool icon: a new window owned by the task "Clock" opens; Project/Quit closes
    it and wbrun reports the message came back.
  - SYS:System/Shell, a project icon with the default tool SYS:System/CLI: a new "AmigaShell" window
    opens (the CLI tool, 38.1 on OS 3.1 and 47.2 on 3.2, reads the icon's WINDOW); EndCLI closes it.

  make build/amiga/wbrun
  UPTERM_RIG=3 python3 tools/rig/rig.py start --max
  UPTERM_RIG=3 python3 tools/rig/wbrun_rig.py
  (the A1200: AMI_HOST/AMI_PORT/AMI_TOKEN as for ami.py)"""
import os, pathlib, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = "RAM:wbrun.out"


def run(cmd, secs=30):
    r = ami.req(0x02, struct.pack('>H', secs) + cmd.encode('latin-1'))
    return struct.unpack('>I', r[:4])[0], r[4:].decode('latin-1')


def windows():
    m = ami._m()
    with ami._conn() as c:
        return [w for _, w in m.ui_windows(m.read_ui(c, every_screen=True))]


def win_id(w):
    # AmigaOS frees a closed window's memory at once and the next window can get the same address (the
    # menu's window closes on Return and the Clock window opens at its address), so the address alone
    # does not tell a new window from an old one: the owning task and the title go with it.
    return (w["addr"], w.get("task"), w.get("title"))


def window_ids():
    return {win_id(w) for w in windows()}


def wait_window(before, match, secs=20):
    """The first window not in `before` (a window_ids() snapshot) that `match` accepts."""
    end = time.time() + secs
    while time.time() < end:
        new = [w for w in windows() if win_id(w) not in before and match(w)]
        if new:
            return new[0]
        time.sleep(1)
    return None


def wait_gone(addr, secs=15):
    end = time.time() + secs
    while time.time() < end:
        if addr not in {w["addr"] for w in windows()}:
            return True
        time.sleep(1)
    return False


def out_text():
    b = ami.read_file(OUT)
    return (b or b"").decode('latin-1').replace("\r", "")


def check(name, ok, detail=""):
    print("%s %s%s" % ("[OK]" if ok else "[FAIL]", name, (": " + detail) if detail else ""))
    return ok


def clock():
    before = window_ids()
    run('Run >NIL: RAM:wbrun >%s SYS:Utilities/Clock' % OUT)
    w = wait_window(before, lambda w: w.get("task") == "Clock")
    ok = check("Clock opens its window", w is not None)
    if not w:
        return False
    print("  window %s %dx%d task %s" % (w["title"], w["width"], w["height"], w["task"]))
    quit_ = [m for m in w.get("menus", []) if ami._m().ui_norm(m["path"]).endswith("quit")]
    if quit_:
        m = ami._m()
        with ami._conn() as c:
            c.send(m.UIACT, struct.pack(">III", 1, int(w["addr"], 16), quit_[0]["code"]))
            c.expect_ok()
    ok &= check("Clock closes on Project/Quit", bool(quit_) and wait_gone(w["addr"]),
                "" if quit_ else "no Quit menu: %s" % [m["path"] for m in w.get("menus", [])])
    time.sleep(2)
    t = out_text()
    ok &= check("wbrun started Clock and got its message back",
                "started Clock" in t and "returned its message" in t, t.strip())
    return ok


def shell():
    before = window_ids()
    rc, out = run('RAM:wbrun SYS:System/Shell')
    print("  " + out.strip().replace("\n", "\n  "))
    w = wait_window(before, lambda w: "amigashell" in w["title"].lower())
    ok = check("Shell icon opens an AmigaShell window", rc == 0 and w is not None, "rc %d" % rc)
    if not w:
        return False
    print("  window %s at %d,%d %dx%d task %s" % (w["title"], w["left"], w["top"], w["width"], w["height"],
                                                  w["task"]))
    m = ami._m()
    with ami._conn() as c:
        m.ui_front(c, w)
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b"EndCLI")
    ami.key(0x44)
    return ok & check("the Shell closes on EndCLI", wait_gone(w["addr"]))


def main():
    ami.put(str(ROOT / "build/amiga/wbrun"), "RAM:wbrun")
    try:
        ok = clock()
        ok &= shell()
    finally:
        run('Delete >NIL: RAM:wbrun %s QUIET' % OUT)
    print("wbrun_rig: %s" % ("OK" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
