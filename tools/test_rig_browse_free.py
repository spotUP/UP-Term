"""tools/rig/upkg_browse_rig.py: the Info free-space parser and the precheck message.

Regression (2026-10-22, rig 4): the 106M System: filled up during the base install, a DOS "volume full"
requester took the keys, and the run reported dead keys. The numbers below are Info on a fresh rig copy.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import upkg_browse_rig as b  # noqa: E402

fails = 0
checks = 0


def check(cond, what):
    global fails, checks
    checks += 1
    if not cond:
        fails += 1
        print("FAIL:", what)


INFO = """
Mounted disks:
Unit\t  Size\t  Used\t  Free Full Errs   Status   Name
PTY:      Unreadable disk
RAM:      455K     455       0 100%   0  Read/Write Ram Disk
DH0:      106M   87076  130146  40%   0  Read/Write System
DH1:     1393M 1791084 1063066  63%   0  Read/Write Rushhours

Volumes available:
Ram Disk [Mounted]
"""
check(b.info_free_blocks(INFO, "DH0:") == 130146, "free blocks by unit name")
check(b.info_free_blocks(INFO, "System:") == 130146, "free blocks by volume name")
check(b.info_free_blocks(INFO, "Rushhours") == 1063066, "volume name without the colon")
check(b.info_free_blocks(INFO.replace("\n", "\r\n"), "rushhours:") == 1063066, "CR LF and case")
check(b.info_free_blocks(INFO, "Ram Disk:") == 0, "a volume name with a space")
check(b.info_free_blocks(INFO, "Nowhere:") is None, "no row: None")
check(b.free_problem(130146, 61 * 1048576, "System:") is None, "63 MB free holds 61 MB")
msg = b.free_problem(130146, 70 * 1048576, "System:")
check(msg and "63.5 MB free" in msg and "70.0 MB" in msg and "requester" in msg, "63 MB free does not hold 70 MB: " + str(msg))
check(b.free_problem(0, 1, "RAM:") is not None, "0 blocks free holds nothing")
check("no row" in b.free_problem(None, 1, "X:"), "an Info without the volume stops the run")
check(b.free_problem(1063066, b.tree_bytes(HERE / "test_rig_browse_free.py"), "Rushhours:") is None, "tree_bytes of a file")

print("[%s] rig_browse_free: %d checks, %d failed" % ("OK" if not fails else "ERROR", checks, fails))
sys.exit(1 if fails else 0)
