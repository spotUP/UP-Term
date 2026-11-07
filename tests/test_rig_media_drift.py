"""RIG-DRIFT-MEDIA: the rig start warns when its UP-Term: tree lacks, or holds a different copy of, kit media
(share/updemo/earth.anim). Rig 91 pointed at a tree from before earth.anim shipped; UPDemo showed the coded globe."""
import pathlib, shutil, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import a1200_drift
import paths
import rig

REL = "share/updemo/earth.anim"


class MediaDrift(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.addCleanup(setattr, rig, "ROOT", rig.ROOT)
        rig.ROOT = self.d
        (self.d / "dist/updemo").mkdir(parents=True)
        for src in a1200_drift.KIT_MEDIA.values():
            (self.d / "dist" / src).write_bytes(b"frames")
        self.vtc = self.d / "vtc"
        (self.vtc / paths.UPTERM_DRAWER).mkdir(parents=True)

    def test_missing_media_warns_with_the_file_name(self):
        self.assertEqual(sorted(rig.media_drift(self.vtc)), sorted((rel, "missing") for rel in a1200_drift.KIT_MEDIA))

    def test_matching_media_is_quiet(self):
        rig.stage_kit_media(self.vtc)
        self.assertEqual(rig.media_drift(self.vtc), [])

    def test_different_media_warns(self):
        rig.stage_kit_media(self.vtc)
        (self.vtc / paths.UPTERM_DRAWER / REL).write_bytes(b"old")
        self.assertEqual(rig.media_drift(self.vtc), [(REL, "differs")])

    def test_installed_kit_place_is_not_judged_by_the_host_drawer(self):
        """RIG-REBOOT-FALSE-MISSING: a kit in the image assigns UP-Term: elsewhere; the empty host drawer says nothing."""
        self.addCleanup(setattr, rig, "clone_upterm_place", rig.clone_upterm_place)
        rig.clone_upterm_place = lambda image: ("DH1", "Tools/UP-Term")
        self.assertEqual(rig.start_media_drift(self.vtc, self.d / "sys.hdf"), [])
        rig.clone_upterm_place = lambda image: None
        self.assertEqual(sorted(rig.start_media_drift(self.vtc, self.d / "sys.hdf")),
                         sorted((rel, "missing") for rel in a1200_drift.KIT_MEDIA))

    def test_os_rig_judges_the_booting_trees_assign_not_sys_hdf(self):
        """MEDIA-DRIFT-OS-TREE: an --os rig boots RIG/os-<OS>, whose User-Startup holds the installer's assign to
        another volume; with no sys.hdf the host drawer must still not be judged."""
        for name in ("RIG", "OS"):
            self.addCleanup(setattr, rig, name, getattr(rig, name))
        rig.RIG = self.d / "rig"
        rig.OS = "32"
        us = rig.RIG / "os-32/S/User-Startup"
        us.parent.mkdir(parents=True)
        us.write_text(';BEGIN UP-Term assign\nIf EXISTS "Empty-12MB-FFS:UP-Term"\n  Assign UP-Term: "Empty-12MB-FFS:UP-Term"\n'
                      'EndIf\n;END UP-Term assign\n')
        self.assertEqual(rig.start_media_drift(self.vtc, self.d / "sys.hdf"), [])

    def test_no_os_and_no_image_still_judges_the_host_drawer(self):
        for name in ("RIG", "OS"):
            self.addCleanup(setattr, rig, name, getattr(rig, name))
        rig.RIG = self.d / "rig"
        rig.RIG.mkdir()
        rig.OS = None
        self.assertEqual(sorted(rig.start_media_drift(self.vtc, self.d / "sys.hdf")),
                         sorted((rel, "missing") for rel in a1200_drift.KIT_MEDIA))


if __name__ == "__main__":
    unittest.main()
