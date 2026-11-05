"""BOOT-31-WB-DEADLOCK source guard (evidence: thoughts/shared/research/s30-evidence/amiberry-hang-vtcon05.md, Step 15).

On OS 3.1 LockPubScreen("Workbench") with the Workbench closed makes Intuition reopen it, and that deadlocks against IPrefs
closing it for the boot RTG switch. Amarchy gets the Workbench lock only through ipd_lock_wb_screen() (iconpen_dos.c), which
looks the screen up in the public screen list first. No other amarchy/*.c may name the Workbench in a LockPubScreen call.

S57-FREEZE-2IN9 (evidence: thoughts/shared/research/s57-evidence/freeze-9boot-rig145.md): the same reopen path runs for a
Workbench node that is listed but still private (LoadWB is opening it), so the helper must also read psn_Flags.
"""
import pathlib, re, unittest

AMARCHY = pathlib.Path(__file__).resolve().parent.parent / "amarchy"
CALL = re.compile(r'LockPubScreen\s*\(\s*(?:\(\s*STRPTR\s*\)\s*)?"Workbench"', re.I)
ALLOWED = {"iconpen_dos.c"}


class WbLockGuard(unittest.TestCase):
    def test_no_direct_workbench_lock(self):
        bad = []
        for f in sorted(AMARCHY.glob("*.c")):
            if f.name in ALLOWED:
                continue
            for n, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
                if CALL.search(line):
                    bad.append(f"{f.name}:{n}")
        self.assertEqual(bad, [], "use ipd_lock_wb_screen(); LockPubScreen(\"Workbench\") reopens a closed Workbench")

    def test_helper_looks_before_it_locks(self):
        src = (AMARCHY / "iconpen_dos.c").read_text()
        body = src[src.index("struct Screen *ipd_lock_wb_screen(void)"):]
        body = body[:body.index("\n}\n")]
        lock = body.index('LockPubScreen((STRPTR)"Workbench")')
        self.assertLess(body.index("wb_node_in"), lock)
        # S57-FREEZE-2IN9: "listed" is not enough, the node must pass Intuition's own test (public) before the call
        self.assertLess(body.index("ip_wb_lockable"), lock)
        self.assertIn("psn_Flags", body[:lock])


if __name__ == "__main__":
    unittest.main()
