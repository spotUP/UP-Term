#!/usr/bin/env python3
"""rig.stage_ixemul (no emulator): VTC:ixp6, first in LIBS:, gets the kit's ixemul.library and ixnet.library
on every start; a stale copy there is replaced (rig 7's 19 Oct ixemul made amarchy-menu stop with "Illegal
instruction", 2026-10-25), --nofpu takes the soft-float ixemul, a current copy is left alone."""
import inspect, pathlib, shutil, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, paths

class Ixp6(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.save = (paths.IXEMUL_LIB, paths.IXEMUL_LIB_SOFT, paths.IXNET_LIB)
        self.addCleanup(lambda: (setattr(paths, "IXEMUL_LIB", self.save[0]),
                                 setattr(paths, "IXEMUL_LIB_SOFT", self.save[1]),
                                 setattr(paths, "IXNET_LIB", self.save[2])))
        for name, body in (("fpu", b"ixemul fpu 200644"), ("soft", b"ixemul soft"), ("net", b"ixnet")):
            (self.d / name).write_bytes(body)
        paths.IXEMUL_LIB, paths.IXEMUL_LIB_SOFT, paths.IXNET_LIB = self.d / "fpu", self.d / "soft", self.d / "net"
        self.vtc = self.d / "vtc"
        (self.vtc / "ixp6").mkdir(parents=True)
        (self.vtc / "ixp6/ixemul.library").write_bytes(b"ixemul stale 199528")

    def lib(self, name):
        return (self.vtc / "ixp6" / name).read_bytes()

    def test_stale_replaced(self):
        rig.stage_ixemul(self.vtc, False)
        self.assertEqual(self.lib("ixemul.library"), b"ixemul fpu 200644")
        self.assertEqual(self.lib("ixnet.library"), b"ixnet")

    def test_nofpu_soft_float(self):
        rig.stage_ixemul(self.vtc, True)
        self.assertEqual(self.lib("ixemul.library"), b"ixemul soft")
        self.assertEqual(self.lib("ixnet.library"), b"ixnet")

    def test_current_left_alone(self):
        rig.stage_ixemul(self.vtc, False)
        before = (self.vtc / "ixp6/ixemul.library").stat().st_mtime_ns
        rig.stage_ixemul(self.vtc, False)
        self.assertEqual((self.vtc / "ixp6/ixemul.library").stat().st_mtime_ns, before)

    def test_start_stages_it(self):
        # start() -> setup_config_only() -> setup(): the boot's staging, beside VTC:amissl
        self.assertIn('stage_ixemul(RIG / "vtc", NOFPU)', inspect.getsource(rig.setup))

if __name__ == "__main__":
    unittest.main()
