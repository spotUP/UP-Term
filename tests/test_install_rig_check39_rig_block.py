"""INSTALL-CHECK39-SHADOW-LEFTOVER: check 39 (S:User-Startup as it was after Uninstall) must not judge the rig's own
';BEGIN rig-upterm-shadows' block. rig.py writes it on every start (and a reboot inside fresh-all re-inserts it after
Install); Uninstall removes the kit's blocks only, so the block stayed and the byte comparison failed."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import user_startup, rig

BLOCK = rig.SHADOWS_UP_BLOCK_TEXT
BEFORE = "; head\n;BEGIN UP-Term assign\nx\n;END UP-Term assign\n"


class Check39RigBlock(unittest.TestCase):
    def test_block_name_is_the_one_rig_writes(self):
        self.assertEqual(rig.SHADOWS_UP_BLOCK, user_startup.RIG_SHADOWS_BLOCK)

    def test_rig_block_left_after_uninstall_is_not_judged(self):
        before = "; head\n"
        after = "; head\n" + BLOCK     # reboot re-inserted the rig block; the kit blocks are gone
        self.assertNotEqual(before, after)
        j = lambda t: user_startup.without_rig_blocks(t.encode("latin-1")).decode("latin-1")
        self.assertEqual(j(after), j(before))

    def test_kit_blocks_and_other_lines_left_by_uninstall_still_differ(self):
        j = lambda t: user_startup.without_rig_blocks(t.encode("latin-1")).decode("latin-1")
        self.assertNotEqual(j("; head\n" + BLOCK + ";BEGIN UP-Term tmp\nAssign TMP: T:\n;END UP-Term tmp\n"), j("; head\n"))
        self.assertNotEqual(j("; head\nstray\n" + BLOCK), j("; head\n"))

    def test_check_startup_after_uninstall_ignores_only_the_rig_block(self):
        # the byte-for-byte check of fresh-all (check 40) reads before/now texts; reboot put the rig block back
        import install_rig
        def run(before, now):
            res = []
            ck = lambda ok, name, detail='': res.append(bool(ok))
            install_rig.check_startup_after_uninstall(ck, before, before, now)
            return res
        self.assertEqual(run("; head\n", "; head\n" + BLOCK), [True])
        self.assertEqual(run("; head\n", "; head\nstray\n" + BLOCK), [False])
        self.assertEqual(run("; head\n", "; head\n" + BLOCK + ";BEGIN UP-Term tmp\nx\n;END UP-Term tmp\n"), [False])

if __name__ == "__main__":
    unittest.main()
