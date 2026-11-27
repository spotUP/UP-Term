"""--a500: the rig clone boots with no UP-Term block in S:User-Startup (RIG-A500-KIT-BOOT, 2026-10-29).

The master image carries the kit's blocks, and the kit is built for the 68020. On the 68000 rig the Patina and
Patina index blocks put up two Software Failure requesters at boot, and installer_rig's D10 case failed on them.
"""
import inspect, pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig
import user_startup

STARTUP = b"""Assign LIBS: SYS:Libs ADD
;BEGIN UP-Term
Assign UP-Term: DH0:UP-Term
;END UP-Term
;BEGIN UP-Term Patina
Run >NIL: UP-Term:bin/Patina
;END UP-Term Patina
;BEGIN UP-Term Patina index
Run >NIL: UP-Term:bin/patina-index
;END UP-Term Patina index
;BEGIN DOpus5
Assign DOpus5: SYS:DOpus5
;END DOpus5
"""


class A500UserStartup(unittest.TestCase):
    def test_every_upterm_block_goes_and_the_rest_stays(self):
        with mock.patch.object(rig, "strip_user_startup_blocks") as strip:
            rig.sys_hdf_a500("sys.hdf")
        image, name = strip.call_args.args
        clean = user_startup.strip_upterm_blocks(STARTUP, name).decode()
        self.assertNotIn("UP-Term", clean)
        self.assertIn("Assign DOpus5: SYS:DOpus5", clean)
        self.assertIn("Assign LIBS: SYS:Libs ADD", clean)

    def test_start_strips_on_both_clone_paths(self):
        src = inspect.getsource(rig._start)   # start() only wraps _start() in start.lock
        self.assertEqual(src.count("sys_hdf_a500(RIG"), 2)
        # after the shadows block (it appends the kit's Patina-Pens block, patina-flat is 68020) on both paths
        for part in src.split("sys_hdf_a500(RIG")[:2]:
            self.assertIn("sys_hdf_shadows_block(RIG", part.rsplit("if A500", 1)[0][-600:])
        self.assertLess(src.index("sys_hdf_a500("), src.index("sys_hdf_cpu_lib("))

    def test_wbstartup_launchers_go(self):
        # ST2-A500-CRASH: patina-index (68020) from SYS:WBStartup gave #80000004 on the A500 rig
        with mock.patch.object(rig, "strip_user_startup_blocks"), \
             mock.patch.object(rig, "sys_hdf_strip_file_blocks") as ss, mock.patch.object(rig.subprocess, "run") as run:
            rig.sys_hdf_a500("sys.hdf")
        ss.assert_called_once_with("sys.hdf", "S/Startup-Sequence", "UP-Term")
        gone = [c.args[0][-1] for c in run.call_args_list if c.args[0][2] == "delete"]
        # the uaegfx monitor too: no graphics card, it only printed "P96 needs an 68020 or better!"
        self.assertEqual(sorted(gone), sorted(list(rig.KIT_WBSTARTUP_ICONS) + ["Devs/Monitors/uaegfx", "Devs/Monitors/uaegfx.info"]))


if __name__ == "__main__":
    unittest.main()
