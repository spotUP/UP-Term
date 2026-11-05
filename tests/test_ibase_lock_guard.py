"""AMARCHY-WIN-IBASE-DOS source guard (evidence: thoughts/shared/research/s58-evidence/win-ibase-dos.md).

Under LockIBase a program may only read IntuitionBase lists. A DOS call (file I/O, config load) or an Intuition call that takes
Intuition locks or waits on another task can deadlock against Intuition or stall input. Every LockIBase ... UnlockIBase span in
amarchy/*.c is scanned; a forbidden call inside one fails the test.

Allowed on purpose: ChangeWindowBox, WindowToFront, WindowToBack and ZipWindow only queue a request to Intuition's input task and
are called under the lock so the window cannot close between the membership check and the call (win.c header, plan 1.3 K6;
tile.c does the same). They are not in the forbidden list.
"""
import pathlib, re, unittest

AMARCHY = pathlib.Path(__file__).resolve().parent.parent / "amarchy"
FORBIDDEN = [
    "amconf_load", "amconf_load_with", "Open", "Close", "Read", "Write", "Lock", "UnLock", "Examine", "ExNext", "ExAll",
    "Delay", "Execute", "SystemTagList", "MoveWindow", "SizeWindow", "ActivateWindow", "ScreenToFront", "ScreenToBack",
    "ShowTitle", "LockPubScreen", "UnlockPubScreen", "OpenWindow", "OpenWindowTagList", "CloseWindow", "OpenScreen",
    "CloseScreen", "OpenLibrary", "rexx_send", "WaitPort", "WaitIO", "DoIO", "SendIO", "VideoControl", "RefreshWindowFrame",
    "CloseWorkbenchObjectA", "WorkbenchControlA",
]
CALL = re.compile(r'(?<![A-Za-z0-9_])(' + "|".join(FORBIDDEN) + r')\s*\(')
COMMENT = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', re.S)


def blank(m):
    return re.sub(r'[^\n]', ' ', m.group(0))


def violations(text):
    clean = COMMENT.sub(blank, text)
    bad = []
    pos = 0
    while True:
        a = clean.find("LockIBase(", pos)
        if a < 0:
            break
        b = clean.find("UnlockIBase(", a)
        if b < 0:
            b = len(clean)
        for m in CALL.finditer(clean, a, b):
            bad.append((clean.count("\n", 0, m.start()) + 1, m.group(1)))
        pos = b + 1
    return bad


class IBaseLockGuard(unittest.TestCase):
    def test_no_dos_or_blocking_call_under_ibase_lock(self):
        bad = []
        for f in sorted(AMARCHY.glob("*.c")):
            for n, name in violations(f.read_text(errors="replace")):
                bad.append(f"{f.name}:{n} {name}()")
        self.assertEqual(bad, [], "gather under LockIBase, unlock, then call DOS or Intuition")

    def test_guard_sees_a_violation(self):
        src = "void f(void){ lock = LockIBase(0);\n amconf_load(&p);\n UnlockIBase(lock);\n amconf_load(&p); }"
        self.assertEqual(violations(src), [(2, "amconf_load")])


if __name__ == "__main__":
    unittest.main()
