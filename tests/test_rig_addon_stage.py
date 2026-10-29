#!/usr/bin/env python3
"""rig.py puts the current C:Claude and C:amiga-pi on every rig (RIG-AGENT-STAGE, vtcon-05 s24, 2026-10-29). Before: both are
add-on programs outside install.dos, so KIT_SHADOWS never listed them: C:Claude stayed a stale 11 Oct build and C:amiga-pi
was missing. Now kitset.ADDONS rows make the boot copy VTC:Claude / VTC:amiga-pi to C: and refresh_kit_shadows stage them
(amiga-pi from its own repo beside vtcon)."""
import contextlib, io, pathlib, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import paths, rig


class AddonStage(unittest.TestCase):
    def test_boot_copies_claude_and_amiga_pi_to_c(self):
        go = rig.kit_shadow_lines(False)
        for n in ("Claude", "amiga-pi"):
            self.assertIn("C:Copy >NIL: VTC:%s C:%s CLONE" % (n, n), go)

    def test_refresh_stages_current_builds_from_their_repos(self):
        self.assertEqual(rig.kit_shadow_src("Claude"), ROOT / "build/amiga/Claude")
        self.assertEqual(rig.kit_shadow_src("amiga-pi"), paths.UPTERM_ROOT / "amiga-pi/build/amiga/amiga-pi")
        vtc = pathlib.Path(tempfile.mkdtemp())
        (vtc / "Claude").write_bytes(b"stale")
        with contextlib.redirect_stdout(io.StringIO()):
            rig.refresh_kit_shadows(vtc)
        for n in ("Claude", "amiga-pi"):
            src = rig.kit_shadow_src(n)
            if src.is_file():
                self.assertEqual((vtc / n).read_bytes(), src.read_bytes())


if __name__ == "__main__":
    unittest.main()
