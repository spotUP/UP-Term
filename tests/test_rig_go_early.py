#!/usr/bin/env python3
"""RIG-GO-EARLY: BOOTX:go runs beside DH0:S/Startup-Sequence (Run >NIL: C:Execute BOOTX:go), so a kit-shadow file Copy
in go overlapped SetPatch and `Copy ENVARC: RAM:ENV`; both CLIs waited on SIGF_DOS for ever (boot stalled at "go started",
rig 96 / 67, s32-evidence/go-stall.md). Invariant: no kit-shadow Copy runs before SetPatch and the ENV: copy finish, so
the shadow pass lives in the S:User-Startup block (BOOTX:shadows-up), which runs after both."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

KIT = b";BEGIN UP-Term Amarchy\nRun >NIL: UP-Term:bin/Amarchy\n;END UP-Term Amarchy\n"


class GoHasNoShadowCopy(unittest.TestCase):
    def test_no_kit_shadow_copy_in_the_early_go_script(self):
        # every shadow destination or backup name is absent from go: the pass is not there
        for src, dest, bak in rig.KIT_SHADOWS:
            self.assertNotIn("VTC:kit-orig/%s" % bak, rig.GO)
            self.assertNotIn("VTC:%s %s CLONE" % (src, rig.q(dest)), rig.GO)
        self.assertNotIn("kit-orig", rig.GO)
        self.assertNotIn("kit shadows done", rig.GO)

    def test_the_user_startup_block_holds_every_shadow_and_follows_the_startup_sequence(self):
        for _, dest, bak in rig.KIT_SHADOWS:
            self.assertIn("VTC:kit-orig/%s" % bak, rig.SHADOWS_UP_SCRIPT)
        # the block exists with and without a kit (never falls back to go)
        for data in (KIT, b"; plain\n"):
            self.assertIn(rig.SHADOWS_UP_BLOCK_TEXT.encode(), rig.user_startup_with_rig_blocks(data))

    def test_handler_mount_follows_the_shadow_pass(self):
        s = rig.SHADOWS_UP_SCRIPT
        self.assertLess(s.rindex("CLONE"), s.index("C:Mount >NIL: XCON:"))
        self.assertNotIn("Mount XCON", rig.GO)

    def test_boot_ready_needs_the_shadow_pass(self):
        self.assertFalse(rig.go_assigns_done("go started\nassigns done\n"))
        self.assertTrue(rig.go_assigns_done("go started\nXCON mounted\nUP-Term shadows done\nassigns done\n"))


if __name__ == "__main__":
    unittest.main()
