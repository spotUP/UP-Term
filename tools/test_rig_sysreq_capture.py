"""RIG5-SYSREQ-CAPTURE: rig.py capture_sysreq logs the System Request body text plus Assign and Info output once."""
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


TREE = ['S 0 640x360 depth=8 "Directory Opus"',
        'W 0 10 10 300x80 active "System Request"',
        'G 1 5 5 200x10 label - "Please replace volume System: in any drive."',
        'G 2 5 40 60x14 button - "Retry"',
        'W 1 0 0 640x300 - "Work"',
        'G 3 0 0 10x10 button - "Other"']
check(rig.sysreq_window_lines(TREE) == TREE[1:4], "sysreq_window_lines keeps the System Request window and its gadgets only")
check(rig.sysreq_window_lines([TREE[0], TREE[4]]) == [], "no System Request: no lines")


class Ami:
    def __init__(self):
        self.cmds = []

    def req(self, code, payload=b""):
        self.cmds.append(payload[2:])
        return b"\0\0\0\0out-of-" + payload[2:]


with tempfile.TemporaryDirectory() as d:
    rig.paths.FSUAE_LOG_DIR = pathlib.Path(d)
    a, st = Ami(), {}
    rig.capture_sysreq(a, TREE[:1], st)
    check(not st and not a.cmds, "nothing logged without a System Request")
    rig.capture_sysreq(a, TREE, st)
    rig.capture_sysreq(a, TREE, st)
    check(a.cmds == [b"Assign", b"Info"], "Assign and Info run once, got %r" % a.cmds)
    text = (pathlib.Path(d) / "late-sysreq.log").read_text()
    check("Please replace volume System:" in text and "out-of-Assign" in text and "out-of-Info" in text, "log has body, Assign, Info")
    check(text.count("first System Request seen") == 1, "logged once")

print("FAIL" if fails else "OK")
sys.exit(1 if fails else 0)
