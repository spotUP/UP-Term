"""BOOT-31-WB-DEADLOCK source guard (evidence: thoughts/shared/research/s30-evidence/amiberry-hang-vtcon05.md, Step 15).

On OS 3.1 LockPubScreen("Workbench") with the Workbench closed makes Intuition reopen it, and that deadlocks against IPrefs
closing it for the boot RTG switch. TE-10.1: the guard is generic. Amarchy and the handler lock a public screen by name only through ipd_lock_pub_screen() (handler/pubscr.c;
ipd_lock_wb_screen() and ipd_lock_default_pub_screen() wrap it), which looks the screen up in the public screen list first.
No other amarchy/*.c may call LockPubScreen( at all, with any argument, 0 and NULL (the default screen) included: on a closed or
private default Workbench that call takes the same reopen path (LOCKPUB-DEFAULT-SCREEN). No other handler/*.c may call it with a name.

S57-FREEZE-2IN9 (evidence: thoughts/shared/research/s57-evidence/freeze-9boot-rig145.md): the same reopen path runs for a
Workbench node that is listed but still private (LoadWB is opening it), so the helper must also read psn_Flags.
"""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
AMARCHY = ROOT / "amarchy"
HANDLER = ROOT / "handler"
PUBSCR = HANDLER / "pubscr.c"
# amarchy/: every LockPubScreen( call counts, the default-screen form LockPubScreen(0) / LockPubScreen(NULL) included
# (LOCKPUB-DEFAULT-SCREEN): callers use ipd_lock_pub_screen() or ipd_lock_default_pub_screen().
CALL = re.compile(r'\bLockPubScreen\s*\(', re.I)
# handler/: a call with a name or a variable counts; the default-screen form is still open there
# (handler/vtcon_handler.c, ledger row HANDLER-LOCKPUB-DEFAULT).
CALL_NAMED = re.compile(r'\bLockPubScreen\s*\(\s*(?!(?:0|NULL)\s*\))', re.I)
ALLOWED = {"pubscr.c"}


class WbLockGuard(unittest.TestCase):
    def test_no_direct_pub_screen_lock(self):
        bad = []
        # FRONTSCR-PUB-LOCK: handler/ is the same rule (frontscr.c locked the front screen's name bare and froze the machine)
        for f in sorted(list(AMARCHY.glob("*.c")) + list(HANDLER.glob("*.c"))):
            if f.name in ALLOWED:
                continue
            for n, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
                if (CALL if f.parent == AMARCHY else CALL_NAMED).search(line):
                    bad.append(f"{f.parent.name}/{f.name}:{n}")
        self.assertEqual(bad, [], "use ipd_lock_pub_screen(name); a bare LockPubScreen reopens a closed Workbench")

    def test_helper_looks_before_it_locks(self):
        src = PUBSCR.read_text()
        body = src[src.index("struct Screen *ipd_lock_pub_screen(const char *name)"):]
        body = body[:body.index("\n}\n")]
        lock = body.index("LockPubScreen((STRPTR)name)")
        self.assertLess(body.index("pubscr_node_in"), lock)
        # S57-FREEZE-2IN9: "listed" is not enough, the node must pass Intuition's own test (public) before the call
        self.assertLess(body.index("ip_pub_lockable"), lock)
        self.assertIn("psn_Flags", body[:lock])


if __name__ == "__main__":
    unittest.main()
