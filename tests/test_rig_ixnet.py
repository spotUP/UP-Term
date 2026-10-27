#!/usr/bin/env python3
"""rig.sys_hdf_ixnet on a small temp image (no emulator): a clone whose Libs/ixnet.library opens AmiTCP:libs gets the
clean paths.IXNET_LIB; a dirty IXNET_LIB refuses the start. start() must call it (source check on the clone sequence)."""
import pathlib, shutil, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, paths

XDF = shutil.which("xdftool")

@unittest.skipUnless(XDF, "needs xdftool (pip install amitools)")
class SysHdfIxnet(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.dirty, self.clean = self.d / "dirty", self.d / "clean"
        self.dirty.write_bytes(b"x" * 50 + b"AmiTCP:libs/usergroup.library\0" + b"y" * 50)
        self.clean.write_bytes(b"c" * 120)
        self.img = self.d / "t.adf"
        subprocess.run([XDF, str(self.img), "create", "+", "format", "T", "+", "makedir", "Libs", "+",
                        "write", str(self.dirty), "Libs/ixnet.library"], check=True, capture_output=True)
        self.save = (paths.IXNET_LIB, rig.RIG)
        rig.RIG = self.d
        self.addCleanup(lambda: (setattr(paths, "IXNET_LIB", self.save[0]), setattr(rig, "RIG", self.save[1])))

    def read(self):
        out = self.d / "read"
        subprocess.run([XDF, str(self.img), "read", "Libs/ixnet.library", str(out)], check=True, capture_output=True)
        return out.read_bytes()

    def test_clone_gets_clean_lib(self):
        paths.IXNET_LIB = self.clean
        rig.sys_hdf_ixnet(self.img)
        self.assertEqual(self.read(), self.clean.read_bytes())

    def test_dirty_source_refuses(self):
        paths.IXNET_LIB = self.dirty
        with self.assertRaises(SystemExit):
            rig.sys_hdf_ixnet(self.img)

    def test_start_calls_it(self):
        src = (ROOT / "tools/rig/rig.py").read_text()
        self.assertIn('sys_hdf_ixnet(RIG / "sys.hdf")', src)

if __name__ == "__main__":
    unittest.main()
