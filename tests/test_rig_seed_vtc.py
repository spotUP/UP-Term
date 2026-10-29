#!/usr/bin/env python3
"""rig.seed_rig_vtc: a rig N>1 drawer that already exists (made by a kit install before setup) still gets
rig 1's files it lacks (VTC:etc, the boot's ETC: assign) and keeps what it holds."""
import pathlib, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

class SeedVtc(unittest.TestCase):
    def test_partial_drawer_is_filled_not_replaced(self):
        d = pathlib.Path(tempfile.mkdtemp())
        src, dst = d / "src", d / "dst"
        (src / "etc").mkdir(parents=True)
        (src / "etc/hosts").write_text("src")
        (src / "vsh").write_text("rig1")
        (dst / "kit-orig").mkdir(parents=True)
        (dst / "vsh").write_text("mine")
        rig.seed_rig_vtc(src, dst)
        self.assertEqual((dst / "etc/hosts").read_text(), "src")
        self.assertEqual((dst / "vsh").read_text(), "mine")
        self.assertTrue((dst / "kit-orig").is_dir())

    def test_missing_drawer_is_copied(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "src/etc").mkdir(parents=True)
        rig.seed_rig_vtc(d / "src", d / "dst")
        self.assertTrue((d / "dst/etc").is_dir())

if __name__ == "__main__":
    unittest.main()
