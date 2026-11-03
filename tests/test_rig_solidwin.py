"""RIG-SOLIDWIN (owner 2026-11-03: "solid windows works ... now keep it in the rigs"): every rig runs SolidWin.

The kit's Install copies solidwin/SolidWin to C:SolidWin and starts it from a ';BEGIN UP-Term SolidWin' block, written
only when S:User-Startup does not spell "C:SolidWin". A rig therefore gets
  - C:SolidWin in its kit set (a1200_drift.DIST_SOURCES -> kitset.derive -> the clone and the boot's shadow row), and
  - a rig-only block that runs BOOTX:solidwin-up, which starts SolidWin only while no "C:SolidWin" line is in
    S:User-Startup. The rig block never spells that name, so Install still writes the kit's block, and after it the
    script leaves the start to that block: SolidWin never runs twice.
"""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import a1200_drift
import kitset
import rig
import user_startup

KIT = (b"; user lines\n"
       b";BEGIN UP-Term assign\nAssign UP-Term: Work:UP-Term\n;END UP-Term assign\n"
       b";BEGIN UP-Term Amarchy\nRun >NIL: UP-Term:bin/Amarchy\n;END UP-Term Amarchy\n")
INSTALLED = KIT + b";BEGIN UP-Term SolidWin\nRun >NIL: C:SolidWin\n;END UP-Term SolidWin\n"


class RigSolidWin(unittest.TestCase):
    def test_the_kit_set_holds_c_solidwin_from_dist(self):
        names = list(a1200_drift.DIST_SOURCES)
        got = dict(a1200_drift.install_map((ROOT / "dist/install.dos").read_text(), names))
        self.assertEqual(got.get("solidwin/SolidWin"), "C:SolidWin")
        self.assertEqual(kitset.clone_path("C:SolidWin"), "C/SolidWin")
        self.assertEqual(rig.kit_shadow_src("solidwin/SolidWin"), ROOT / "dist/solidwin/SolidWin")
        self.assertIn(("solidwin/SolidWin", "C:SolidWin", "C-SolidWin"), rig.KIT_SHADOWS)

    def test_the_block_sits_before_the_kit_blocks_once(self):
        out = rig.user_startup_with_rig_blocks(KIT).decode("latin-1")
        self.assertIn(";BEGIN rig-solidwin\nExecute BOOTX:solidwin-up\n;END rig-solidwin\n", out)
        self.assertLess(out.index(";BEGIN rig-solidwin"), out.index(";BEGIN UP-Term assign"))
        self.assertEqual(rig.user_startup_with_rig_blocks(out.encode("latin-1")).decode("latin-1"), out)   # once

    def test_the_rig_block_never_spells_the_name_install_searches_for(self):
        # install.dos: `Search >NIL: S:User-Startup "C:SolidWin"` / `If WARN` writes the kit's block
        install = (ROOT / "dist/install.dos").read_text()
        self.assertIn('Search >NIL: S:User-Startup "C:SolidWin"', install)
        self.assertNotIn("C:SolidWin", rig.SOLIDWIN_UP_BLOCK_TEXT)
        self.assertNotIn(b"C:SolidWin", rig.user_startup_with_rig_blocks(KIT))

    def test_the_script_starts_it_only_without_a_kit_line(self):
        s = rig.SOLIDWIN_UP_SCRIPT
        self.assertIn('Search >NIL: S:User-Startup "C:SolidWin"\nIf WARN\n', s)
        self.assertLess(s.index("If WARN"), s.index("Run >NIL: <NIL: C:SolidWin"))
        self.assertIn("VTC:solidwin/SolidWin", s)        # an --os tree has no clone copy

    def test_the_install_check_filter_ignores_the_block(self):
        self.assertIn("rig-solidwin", user_startup.RIG_BLOCKS)
        self.assertEqual(user_startup.without_rig_blocks(rig.user_startup_with_rig_blocks(INSTALLED)), INSTALLED)

    def test_start_writes_the_script(self):
        body = pathlib.Path(rig.__file__).read_text()
        self.assertIn('(RIG / "boot/solidwin-up").write_text(SOLIDWIN_UP_SCRIPT)', body)


if __name__ == "__main__":
    unittest.main()
