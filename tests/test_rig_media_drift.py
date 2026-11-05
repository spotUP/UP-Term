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


if __name__ == "__main__":
    unittest.main()
