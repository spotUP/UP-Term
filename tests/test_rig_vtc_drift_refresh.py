"""The rig start and the menu rig's stale_guard bring VTC: to the build, from the same file set build_drift checks
(RIG-START-VTC-DRIFT, MENU-STALE-RESTAGE-DRIFT).

Before: only `rig.py install` copied the build/amiga programs; the start and the restage only warned, so every fresh
start printed "[WARN] 16 file(s) in VTC: differ" and stale_guard's restage (a different, shorter list) left
"hl, mdv, rz, sz, vsh". Now program_pairs is the one list: build_drift reads it, install, setup and stale_guard copy it.
"""
import pathlib, shutil, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import paths
import rig
import amarchy_menu_rig as m


class VtcRefresh(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        for a in ("ROOT",):
            self.addCleanup(setattr, rig, a, getattr(rig, a))
        rig.ROOT = self.d
        (self.d / "build/amiga").mkdir(parents=True)
        for n in ("vsh", "hl", "mdv", "rz", "sz"):
            (self.d / "build/amiga" / n).write_bytes(b"new " + n.encode())
        self.vtc = self.d / "rig/vtc"
        bin_ = self.vtc / paths.UPTERM_DRAWER / "bin"
        bin_.mkdir(parents=True)
        for n in ("vsh", "hl", "mdv", "rz", "sz"):
            (self.vtc / n).write_bytes(b"old")
            (bin_ / n).write_bytes(b"old")

    def test_refresh_clears_drift_from_the_same_pairs(self):
        self.assertEqual(len(rig.build_drift(self.vtc)), 10)
        rig.refresh_programs(self.vtc, self.d / "build/amiga")
        self.assertEqual(rig.build_drift(self.vtc), [])
        self.assertEqual((self.vtc / "hl").read_bytes(), b"new hl")

    def test_refresh_keeps_absent_files_absent(self):
        (self.vtc / "sz").unlink()
        rig.refresh_programs(self.vtc, self.d / "build/amiga")
        self.assertFalse((self.vtc / "sz").exists())

    def test_stale_guard_restage_leaves_no_drift(self):
        with mock.patch.object(paths, "RIG", str(self.d / "rig")), \
             mock.patch.object(rig, "refresh_kit_shadows"):
            self.assertIsNone(m.stale_guard(lambda: None))

    def test_setup_refreshes_before_the_warn(self):
        import inspect
        src = inspect.getsource(rig.setup)
        self.assertLess(src.index("refresh_programs("), src.index("build_drift(RIG"))


if __name__ == "__main__":
    unittest.main()
