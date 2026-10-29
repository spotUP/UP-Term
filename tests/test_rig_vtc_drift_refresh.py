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

    def test_refresh_brings_the_dist_userland_in_the_drawer_bin(self):
        # RJ7-CURL-SHADOW: VTC:Apps/bin/curl stayed the 62b96743 one while the dist held adf34ce1; the start's
        # refresh copied only build/amiga programs there, and the boot's shadow wrote only into what UP-Term: names
        ul = self.d / "dist-userland"
        ul.mkdir()
        (ul / "curl").write_bytes(b"new curl")
        (ul / "wget").write_bytes(b"new wget")
        curl = self.vtc / paths.UPTERM_DRAWER / "bin/curl"
        curl.write_bytes(b"old curl")
        with mock.patch.object(rig, "USERLAND_SRC", ul):
            self.assertIn("%s/bin/curl" % paths.UPTERM_DRAWER, rig.build_drift(self.vtc))
            rig.refresh_programs(self.vtc, self.d / "build/amiga")
            self.assertEqual(rig.build_drift(self.vtc), [])
        self.assertEqual(curl.read_bytes(), b"new curl")
        self.assertFalse((self.vtc / "curl").exists())   # userland goes in the drawer's bin only
        self.assertFalse((self.vtc / paths.UPTERM_DRAWER / "bin/wget").exists())

    def test_a_name_both_built_and_in_the_userland_has_one_source(self):
        # build_drift listed Apps/bin/md5sum twice when build/amiga and the dist's userland both held md5sum, and
        # refresh_programs copied the build, then the userland copy over it, so the drift never cleared
        ul = self.d / "dist-userland"
        ul.mkdir()
        (ul / "hl").write_bytes(b"userland hl")
        with mock.patch.object(rig, "USERLAND_SRC", ul):
            drift = rig.build_drift(self.vtc)
            self.assertEqual(drift.count("%s/bin/hl" % paths.UPTERM_DRAWER), 1)
            rig.refresh_programs(self.vtc, self.d / "build/amiga")
            self.assertEqual(rig.build_drift(self.vtc), [])
        self.assertEqual((self.vtc / paths.UPTERM_DRAWER / "bin/hl").read_bytes(), b"new hl")

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
