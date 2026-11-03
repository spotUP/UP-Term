"""RIG-DRIFT-MEDIA: the rig start warns when its UP-Term: tree lacks, or holds a different copy of, kit media
(share/updemo/earth.anim). Rig 91 pointed at a tree from before earth.anim shipped; UPDemo showed the coded globe."""
import pathlib, shutil, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

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
        (self.d / "dist/updemo/earth.anim").write_bytes(b"frames")
        self.vtc = self.d / "vtc"
        (self.vtc / paths.UPTERM_DRAWER).mkdir(parents=True)

    def test_missing_media_warns_with_the_file_name(self):
        self.assertEqual(rig.media_drift(self.vtc), [(REL, "missing")])

    def test_matching_media_is_quiet(self):
        rig.stage_kit_media(self.vtc)
        self.assertEqual(rig.media_drift(self.vtc), [])

    def test_different_media_warns(self):
        rig.stage_kit_media(self.vtc)
        (self.vtc / paths.UPTERM_DRAWER / REL).write_bytes(b"old")
        self.assertEqual(rig.media_drift(self.vtc), [(REL, "differs")])


if __name__ == "__main__":
    unittest.main()
