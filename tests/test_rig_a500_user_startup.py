"""--a500: the rig clone boots with no UP-Term block in S:User-Startup (RIG-A500-KIT-BOOT, 2026-10-29).

The master image carries the kit's blocks, and the kit is built for the 68020. On the 68000 rig the Amarchy and
Amarchy index blocks put up two Software Failure requesters at boot, and installer_rig's D10 case failed on them.
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
;BEGIN UP-Term Amarchy
Run >NIL: UP-Term:bin/Amarchy
;END UP-Term Amarchy
;BEGIN UP-Term Amarchy index
Run >NIL: UP-Term:bin/amarchy-index
;END UP-Term Amarchy index
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
        src = inspect.getsource(rig.start)
        self.assertEqual(src.count("if A500:\n            sys_hdf_a500("), 2)
        self.assertLess(src.index("sys_hdf_a500("), src.index("sys_hdf_cpu_lib("))


if __name__ == "__main__":
    unittest.main()
