"""amarchy_gui_rig.uhc_catalogue: the staged UHC stub is installable on a rig that has no UHC add-on.

Rig 4 (research/2026-10-28-s11-uhc-text-cases.md): `upkg install uhc:pg8uhc` failed rc 14 "UHC:C: device (or volume) is not
mounted"; and the stub had no `Copy ... TO <file>` line, so `upkg remove uhc:` could not tell which files it installed (rc 20).
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_gui_rig as g


class UhcCatalogue(unittest.TestCase):
    def stage(self):
        cmds, puts = [], {}
        with mock.patch.object(g.c, "run", side_effect=lambda cmd, *a: cmds.append(cmd) or (0, "")), \
             mock.patch.object(g.ami, "put", side_effect=lambda src, dst: puts.__setitem__(dst, open(src).read())):
            drop = g.uhc_catalogue()
            staged = len(cmds)
            drop()
        return cmds[:staged], cmds[staged:], puts

    def test_assign_c_drawer_and_copy_to_line(self):
        cmds, undo, puts = self.stage()
        self.assertIn("MakeDir SYS:UHC/C", cmds)
        self.assertIn("MakeDir SYS:UHC/S", cmds)
        self.assertIn("Assign UHC: SYS:UHC", cmds)
        script = puts["SYS:UHC/Packages/" + g.PKG_UHC]
        self.assertIn("Packages/Installed/" + g.PKG_UHC, script)
        self.assertRegex(script, r"(?m)^Copy \S+ TO UHC:C/\S+$")

    def test_cleanup_removes_assign_and_drawer(self):
        _, undo, _ = self.stage()
        self.assertEqual(undo[0], "Assign UHC: REMOVE")
        self.assertTrue(undo[1].startswith("Delete >NIL: SYS:UHC"))


if __name__ == "__main__":
    unittest.main()
