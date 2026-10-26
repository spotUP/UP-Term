#!/usr/bin/env python3
"""rig.sys_hdf_cpu_lib on a small temp image (no emulator): --nofpu removes Libs/68060.library, a 060 with FPU
writes MMULib's set (68060, mmu, 680x0) and replaces an older 68060.library the image already has (Phase5's 44.3
reset OS 3.1 in a loop, rig 8 2026-10-26), other CPUs leave the image alone, the 68060.library.sig neighbour is
never taken for it."""
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
        self.mmu, self.x0 = self.d / "mmu", self.d / "x0"
        self.mmu.write_bytes(b"m" * 200)
        self.x0.write_bytes(b"k" * 100)
        self.img = self.d / "t.adf"
        subprocess.run([XDF, str(self.img), "create", "+", "format", "T", "+", "makedir", "Libs", "+",
                        "write", str(self.lib), "Libs/68060.library.sig"], check=True, capture_output=True)
        self.save = (rig.M060, rig.NOFPU, paths.LIB68060, paths.LIBMMU, paths.LIB680X0)
        paths.LIB68060, paths.LIBMMU, paths.LIB680X0 = self.lib, self.mmu, self.x0
        self.addCleanup(lambda: (setattr(rig, "M060", self.save[0]), setattr(rig, "NOFPU", self.save[1]),
                                 setattr(paths, "LIB68060", self.save[2]), setattr(paths, "LIBMMU", self.save[3]),
                                 setattr(paths, "LIB680X0", self.save[4])))

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

    def read(self, name):
        out = self.d / ("read-" + name)
        subprocess.run([XDF, str(self.img), "read", "Libs/" + name, str(out)], check=True, capture_output=True)
        return out.read_bytes()

    def test_replaces_old_lib_and_adds_mmulib(self):
        old = self.d / "phase5"
        old.write_bytes(b"p" * 400)
        subprocess.run([XDF, str(self.img), "write", str(old), "Libs/68060.library"], check=True, capture_output=True)
        self.assertTrue({"68060.library", "mmu.library", "680x0.library"} <= set(self.run_cpu(True, False)))
        self.assertEqual(self.read("68060.library"), self.lib.read_bytes())
        self.assertEqual(self.read("mmu.library"), self.mmu.read_bytes())
        self.assertEqual(self.read("680x0.library"), self.x0.read_bytes())

if __name__ == "__main__":
    unittest.main()
