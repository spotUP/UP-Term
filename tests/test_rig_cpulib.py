#!/usr/bin/env python3
"""rig.sys_hdf_cpu_lib on a small temp image (no emulator): --nofpu removes Libs/68060.library, a 060 with FPU
writes it when missing, other CPUs leave the image alone, the 68060.library.sig neighbour is never taken for it."""
import pathlib, shutil, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, paths

XDF = shutil.which("xdftool")

@unittest.skipUnless(XDF, "needs xdftool (pip install amitools)")
class CpuLib(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.lib = self.d / "lib"
        self.lib.write_bytes(b"x" * 300)
        self.img = self.d / "t.adf"
        subprocess.run([XDF, str(self.img), "create", "+", "format", "T", "+", "makedir", "Libs", "+",
                        "write", str(self.lib), "Libs/68060.library.sig"], check=True, capture_output=True)
        self.save = (rig.M060, rig.NOFPU, paths.LIB68060)
        paths.LIB68060 = self.lib
        self.addCleanup(lambda: (setattr(rig, "M060", self.save[0]), setattr(rig, "NOFPU", self.save[1]),
                                 setattr(paths, "LIB68060", self.save[2])))

    def libs(self):
        out = subprocess.run([XDF, str(self.img), "list", "Libs"], capture_output=True, text=True).stdout
        return [l.split()[0] for l in out.splitlines() if l.split()]

    def run_cpu(self, m060, nofpu):
        rig.M060, rig.NOFPU = m060, nofpu
        rig.sys_hdf_cpu_lib(self.img)
        return self.libs()

    def test_follows_machine(self):
        self.assertNotIn("68060.library", self.run_cpu(False, False))   # 020: untouched
        self.assertIn("68060.library", self.run_cpu(True, False))       # 060 with FPU: written
        self.assertNotIn("68060.library", self.run_cpu(True, True))     # 68LC060: removed
        self.assertIn("68060.library.sig", self.libs())

if __name__ == "__main__":
    unittest.main()
