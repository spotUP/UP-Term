"""tools/rig/ami.py over wasabid, on the host: every function the rig importers use, against wasabi's own mock
daemon (build/wasabi/src/tests/mock-wasabid.py, the pinned upstream clone: `make wasabi` first).

The mock serves a 640x256 screen (row scale 2) with a requester window "Question" (task RequestChoice) at 200,50 200x60, gadgets and a
menu strip, a 8x4 grab, and logs what it is sent (keys.log, ui.log), so each function is checked for the legacy
answer shape the importers parse and for the wire frame it produced. Reachability on iron is a rig run (README)."""
import os
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import wasabi as rigwasabi  # noqa: E402

MOCK = rigwasabi.build_dir() / "src/tests/mock-wasabid.py"
if not MOCK.is_file():
    raise SystemExit("test_rig_ami: %s missing: run `make wasabi`" % MOCK)
with socket.socket() as s:
    s.bind(("127.0.0.1", 0))
    PORT = s.getsockname()[1]
KEY = "hunter2"
os.environ.update(AMI_HOST="127.0.0.1", AMI_PORT=str(PORT), AMI_TOKEN=KEY)
ROOT = tempfile.mkdtemp(prefix="ami-mock-")
env = dict(os.environ, XDG_CACHE_HOME=ROOT + "/c", XDG_CONFIG_HOME=ROOT + "/f")
srv = subprocess.Popen([sys.executable, "-I", str(MOCK), "--root", ROOT, "--port", str(PORT), "--key", KEY],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
for _ in range(100):
    try:
        socket.create_connection(("127.0.0.1", PORT), 0.2).close()
        break
    except OSError:
        time.sleep(0.05)

import ami  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


def log(name):
    p = pathlib.Path(ROOT) / name
    return p.read_text().splitlines() if p.exists() else []


def gap(fn, *a):
    try:
        fn(*a)
    except ami.Gap as e:
        return str(e)
    return None


try:
    # req 1 ping, 6 info
    check(b"wasabid" in ami.req(0x01).lower() or len(ami.req(0x01)) > 0, "ping answers the banner")
    # req 2 exec: rc + output
    b = ami.req(0x02, struct.pack(">H", 20) + b"echo hi")
    check(struct.unpack(">I", b[:4])[0] == 0 and b[4:].strip() == b"hi", "exec: rc 0 and the output: %r" % b)
    b = ami.req(0x02, struct.pack(">H", 20) + b"sh -c 'exit 5'")
    check(struct.unpack(">I", b[:4])[0] == 5, "exec: a failing command keeps its return code")
    # put / read_file / get / restore_file
    src = pathlib.Path(ROOT) / "local.bin"
    data = bytes(range(256)) * 300
    src.write_bytes(data)
    ami.put(str(src), "RAM:t.bin")
    check(ami.read_file("RAM:t.bin") == data, "put then read_file round-trips 76800 bytes")
    check(ami.req(0x03, b"RAM:t.bin") == data, "req 3 get")
    check(ami.read_file("RAM:nonexist") is None, "read_file: a missing file is None, not an exit")
    # (restore_file(None) sends AmigaDOS `Delete`, which the host mock cannot run: that case is a rig run)
    ami.restore_file("RAM:d/t.bin", b"abc")
    check(ami.read_file("RAM:d/t.bin") == b"abc", "restore_file(bytes) puts it back")
    ami.req(0x04, struct.pack(">H", 8) + b"RAM:r.bi" + b"xyz")
    check(ami.read_file("RAM:r.bi") == b"xyz", "req 4 put")
    # req 0x0D uitree and window()
    tree = ami.req(0x0D).decode("latin-1").splitlines()
    check(tree[0] == 'S 0 640x256 depth=2 "Workbench Screen"', "uitree: screen line: %r" % tree[0])
    check(any(l.startswith("W 0 200 50 200x60 active ") and l.endswith('"Question"') for l in tree),
          "uitree: window line in screen pixels (rows not doubled): %r" % tree[1:3])
    check('G 1 210 90 40x14 button - "Yes"' in tree, "uitree: gadget line absolute, id first: %r" % tree[2:6])
    check('G 3 310 90 60x14 button disabled "Never"' in tree, "uitree: a ghosted gadget says disabled")
    check('G 4 210 70 150x14 string - "Name" "Bob"' in tree, "uitree: a string gadget carries its value")
    w = ami.window("Question")
    check(w and w["box"] == (200, 50, 200, 60) and w["active"] and "sys:close" in w, "window(): box, active, sys:close: %r" % w)
    check(ami.window("Nothing here") is None, "window(): an absent title is None")
    # req 0x0F menus: M/I lines
    menus = ami.req(0x0F).decode("latin-1").splitlines()
    check(menus[0] == 'W "Question"', "menus: window line: %r" % menus[:2])
    check(any(l.startswith("M 0 on ") and l.endswith('"Project"') for l in menus), "menus: M line")
    q = [l for l in menus if l.endswith('"Quit"')]
    check(q and q[0].split()[:4] == ["I", "0", "2", "-"] and '"Q"' in q[0], "menus: I line: item 2, not a sub item, key Q: %r" % q)
    check(any("disabled" in l and l.endswith('"Print"') for l in menus), "menus: a disabled item")
    # grab / shot / png
    w_, h_, rows = ami.grab()
    check((w_, h_, len(rows)) == (8, 4, 4) and all(len(r) == 24 for r in rows), "grab(): 8x4 RGB rows")
    check(rows[1][3:6] == bytes((32, 64, 128)), "grab(): pixel (1,1) of the mock gradient")
    w2, h2, r2 = ami.grab((2, 1, 3, 2))
    check((w2, h2) == (3, 2) and r2[0] == rows[1][6:15] and r2[1] == rows[2][6:15], "grab(box): the region is a crop of the full grab")
    out = pathlib.Path(ROOT) / "s.png"
    ami.png(str(out), w_, h_, rows)
    check(out.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n", "png(): a PNG file")
    ami.main(["shot", str(out), "0", "0", "4", "4"])
    check(out.is_file(), "main shot with a region")
    # key: raw code and qualifier, down and up in one frame
    ami.key(0x44)
    ami.key(0x45, 0x8)
    k = log("keys.log")
    check(k[-2] == "0000" + "00440000" + "00c40000", "key(0x44): one frame, down then up: %s" % k[-2:])
    check(k[-1] == "0000" + "00450008" + "00c50008", "key(code, qual): the qualifier rides both events: %s" % k[-1:])
    # type
    ami.req(0x08, bytes([4]) + b"dir\r")
    check(log("keys.log")[-1] == "0001" + b"dir\r".hex(), "type: KEY op 1 with the text")
    # pointer_scale, click_px, script
    check(ami.pointer_scale() == (1.0, 2.0), "pointer_scale(): rows doubled on 640x256: %r" % (ami.pointer_scale(),))
    ami.click_px(10, 20)
    check(log("ui.log")[-1] == "mouse 1 0 1 10 40", "click_px(x, y): a left click at (x, 2y): %s" % log("ui.log")[-1])
    ami.script(("move", 100, 80), ("wait", 1), ("button", 0, 1), ("wait", 1), ("button", 0, 0))
    u = log("ui.log")[-3:]
    check(u == ["mouse 0 0 0 100 80", "mouse 2 0 0 -32768 -32768", "mouse 3 0 0 -32768 -32768"], "script(): move, down, up frames: %s" % u)
    ami.req(0x08, bytes([1]) + struct.pack(">HH", 7, 9))
    check(log("ui.log")[-1] == "mouse 0 0 0 7 9", "INPUT move")
    # uiact: click by label, by id, dclick; menushort; settext
    ami.req(0x0E, b"click\tQuestion\tYes")
    check(log("ui.log")[-1].startswith("mouse 1 0 1 "), "uiact click by label: a mouse click")
    r = ami.req(0x0E, b"click\tQuestion\t2")
    check(r.startswith(b"click at ") and log("ui.log")[-1].startswith("mouse 1 0 1 "), "uiact click by gadget id: %r" % r)
    ami.req(0x0E, b"dclick\tQuestion\tNo")
    check(log("ui.log")[-1].startswith("mouse 1 0 2 "), "uiact dclick: count 2")
    ami.main(["ui", "menushort", "", "Q"])
    check(len(log("keys.log")[-1]) > 8, "uiact menushort: right Amiga + key as raw events")
    ami.req(0x0E, b"settext\tQuestion\tName\t42")
    check(log("keys.log")[-1] == "0001" + b"42\r".hex(), "uiact settext: focus, clear, type text and Return")
    # rexx / screens
    b = ami.req(0x12, b"return 1")
    check(len(b) >= 4 and struct.unpack(">I", b[:4])[0] == 10, "rexx: rc u32 + result (the mock answers rc 10 to a program)")
    check(ami.req(0x0A)[0] == 2, "screens: two screens in the legacy binary shape")
    # POINTER: the mock's WINDOWS S line carries wasabid 0.10's two extra fields (MouseX 320, MouseY 100)
    check(ami.req(0x0B) == struct.pack(">HH", 320, 100), "POINTER: the screen's MouseX, MouseY in screen pixels: %r" % (ami.req(0x0B),))
    check(ami.pointer_scale() == (1.0, 2.0), "pointer_scale() still (1, row_scale) beside POINTER")
    # gaps are named, not silent
    for what, fn, a in (("INPUT operation", ami.req, (0x08, bytes([77]))), ("UIACT verb", ami.req, (0x0E, b"zap\tx\ty"))):
        g = gap(fn, *a)
        check(g and g.startswith("ami.py gap: ") and what in g, "gap named: %s -> %r" % (what, g))
    # BREAK ends a running exec (Ctrl-C to its slot), the exec returns, and BREAK with nothing running is a no-op
    import threading
    res = []
    th = threading.Thread(target=lambda: res.append(ami.req(0x02, struct.pack(">H", 0) + b"sleep 30")))
    t0 = time.time()
    th.start()
    time.sleep(1.0)
    check(ami.req(0x09) == b"", "BREAK: answers empty")
    th.join(10)
    check(not th.is_alive() and time.time() - t0 < 10, "BREAK: the running exec ended well before its 30 s")
    check(ami.req(0x09) == b"", "BREAK with nothing running is a no-op")
    # requesters: which windows count (the fake read_ui answers are the shapes wasabid sends)
    def win(title, task, *gads):
        return {"addr": title, "title": title, "task": task,
                "gadgets": [{"kind": k, "label": l} for k, l in gads]}
    screens = [{"title": "Workbench Screen", "windows": [
        win("System Request", "vsh", ("button", "Retry"), ("button", "Cancel")),
        win("ixemul.library message", "tmux", ("button", "Abort")),
        win("Program message", "jq", ("label", "free: start of block corrupt!"), ("button", "Continue"), ("button", "Abort")),
        win("UP-Term Prefs", "UPTermPrefs", ("button", "Save"), ("button", "Use"), ("button", "Cancel")),
        win("Find", "ed", ("string", "Text"), ("button", "OK"), ("close", ""), ("depth", "")),
        win("Workbench", "Workbench")]}]
    got = [r["title"] for r in ami.requesters_in(screens)]
    check(got == ["System Request", "ixemul.library message", "Program message"],
          "requesters_in: DOS, ixemul panic and an Abort/Continue window are requesters; Prefs, a find box, a "
          "backdrop are not: %r" % got)
    check(ami.describe_requester(ami.requesters_in(screens)[0]) ==
          '"System Request" (task vsh, screen "Workbench Screen", buttons Retry|Cancel)', "describe_requester")
    check(isinstance(ami.requesters(), list), "requesters(): reads every screen of the mock")
    # an EXEC blocked by a requester it opened ends with an ERR naming it, and the log gets a line. The watch shuts the
    # EXEC's socket down; a recv already waiting does not always wake on that (macOS), so the bound is wasabi's
    # RUN_CHECK (10 s, the run loop's own wake-up) plus slack, not the 0.2 s poll
    blocker = {"addr": "r1", "title": "System Request", "task": "vsh", "screen": "Workbench Screen",
               "buttons": ["Retry", "Cancel"]}
    real, poll, rlog = ami.requesters, ami.REQUESTER_POLL, ami.REQUESTER_LOG
    looks = []
    ami.requesters = lambda: looks.append(1) or ([] if len(looks) == 1 else [blocker])
    ami.REQUESTER_POLL = 0.2
    ami.REQUESTER_LOG = pathlib.Path(ROOT) / "requesters.log"
    t0 = time.time()
    try:
        ami.req(0x02, struct.pack(">H", 0) + b"sleep 30")
        err = ""
    except SystemExit as e:
        err = str(e)
    check(err.startswith("ERR: sleep 30 is blocked by the requester \"System Request\" (task vsh") and
          time.time() - t0 < 15, "a requester opened by the command ends the EXEC with an ERR: %r after %.1f s" % (err, time.time() - t0))
    check(any("sleep 30 blocked by \"System Request\"" in l for l in log("requesters.log")),
          "requesters.log names the command and the requester: %r" % log("requesters.log"))
    # a requester already open before the command is not the command's; a timeout still names it
    ami.requesters = lambda: [blocker]
    try:
        ami.req(0x02, struct.pack(">H", 2) + b"sleep 30")
        err = ""
    except SystemExit as e:
        err = str(e)
    check(err.startswith("ERR: ") and "open requesters: \"System Request\"" in err and "blocked by" not in err,
          "a timeout names the open requester, one there before the command does not end it early: %r" % err)
    ami.requesters, ami.REQUESTER_POLL, ami.REQUESTER_LOG = real, poll, rlog
    ami.req(0x09)
    # a bad key is an ERR exit like amiagent's, a dead port is not ERR
    try:
        ami.req(0x03, b"RAM:no/such")
    except SystemExit as e:
        check(str(e).startswith("ERR"), "a missing file is an ERR exit: %r" % str(e))
    # reboot(): marker in RAM:, C:Reboot, down while it boots, back when the marker is gone
    real_req, sent, state = ami.req, [], {"marker": False, "down": 0}
    def fake_req(code, payload=b"", timeout=150):
        cmd = payload[2:].decode("latin-1")
        sent.append(cmd)
        if state["down"]:
            state["down"] -= 1
            raise SystemExit("wasabid: connection refused")
        if cmd.startswith("Echo >"):
            state["marker"] = True
        elif cmd == "Run >NIL: C:Reboot":
            state["marker"], state["down"] = False, 2
        rc = 0 if not cmd.startswith("List ") or state["marker"] else 20
        return struct.pack(">I", rc)
    ami.req = fake_req
    try:
        ami.reboot(wait=5, every=0)
        check(sent[0].startswith("Echo >" + ami.REBOOT_MARKER) and sent[1] == "Run >NIL: C:Reboot" and len(sent) == 5,
              "reboot: marker, C:Reboot, two refused polls, marker gone: %r" % sent)
        state.update(marker=False, down=0); sent.clear()
        ami.req = lambda code, payload=b"", timeout=150: (sent.append(1), struct.pack(">I", 0))[1]
        try:
            ami.reboot(wait=0.05, every=0.01)
            check(False, "reboot: a marker that never goes away is an error")
        except SystemExit as e:
            check("did not come back" in str(e), "reboot: no restart is an error: %r" % str(e))
    finally:
        ami.req = real_req
finally:
    srv.terminate()
    srv.wait()
    import shutil
    shutil.rmtree(ROOT, ignore_errors=True)

print("rig ami.py over wasabid: %s" % ("%d FAILED" % fails if fails else "all passed"))
sys.exit(1 if fails else 0)
