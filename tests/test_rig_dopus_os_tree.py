"""RIG-DOPUS-OS32: a rig on a host-directory system (--os 32, --os32) gets Directory Opus 5.104 as the sys.hdf clone does.

sys_hdf_dopus ran only for the sys.hdf clone, so `--os 32` rigs had no DOPUS.1 port. os_tree_dopus puts the Installer's
User-Startup block and ENVARC files into the tree from the rig's VTC: install; --no-dopus strips them again.
"""
import pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig

BLOCK = b';BEGIN DOpus5\nAssign DOpus5: "VTCX:DOpus5"\nResident LoadWB DOpus5:C/LoadDB PURE\n;END DOpus5\n'


class OsTreeDopus(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.vtc = self.tmp / "rig/vtc"
        (self.vtc / "DOpus5").mkdir(parents=True)
        (self.vtc / "DOpus5/DirectoryOpus").write_text("x")
        (self.vtc / rig.DOPUS_BLOCK).write_bytes(BLOCK)
        (self.vtc / rig.DOPUS_ENVARC).mkdir()
        (self.vtc / rig.DOPUS_ENVARC / "DOpus5Startup").write_text("s")
        (self.vtc / rig.DOPUS_STAMP).write_text("abc\n")
        self.tree = self.tmp / "os32"
        (self.tree / "S").mkdir(parents=True)
        (self.tree / "S/User-Startup").write_bytes(b"Assign VTC: VTCX:\n")
        self.p = [mock.patch.object(rig, "RIG", self.tmp / "rig"), mock.patch.object(rig, "dopus_archive_md5", lambda: "abc")]
        for p in self.p:
            p.start()

    def tearDown(self):
        for p in self.p:
            p.stop()

    def test_block_and_envarc_go_in_once(self):
        rig.os_tree_dopus(self.tree)
        rig.os_tree_dopus(self.tree)   # the tree keeps earlier runs: a second start does not double the block
        data = (self.tree / "S/User-Startup").read_bytes()
        self.assertEqual(data.count(b";BEGIN DOpus5"), 1)
        self.assertTrue(data.startswith(b"Assign VTC: VTCX:\n"))
        self.assertEqual((self.tree / "Prefs/Env-Archive/DOpus5Startup").read_text(), "s")

    def test_no_dopus_strips_it(self):
        rig.os_tree_dopus(self.tree)
        rig.os_tree_no_dopus(self.tree)
        self.assertNotIn(b"DOpus5", (self.tree / "S/User-Startup").read_bytes())

    def test_stale_stamp_changes_nothing(self):
        (self.vtc / rig.DOPUS_STAMP).write_text("old\n")
        rig.os_tree_dopus(self.tree)
        self.assertEqual((self.tree / "S/User-Startup").read_bytes(), b"Assign VTC: VTCX:\n")


if __name__ == "__main__":
    unittest.main()
