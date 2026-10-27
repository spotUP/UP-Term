#!/usr/bin/env python3
"""rig.sys_hdf_kit on a small temp image (no emulator, RIG-KIT-DERIVE): a clone holding older copies ends up equal to
the DERIVED set (kitset.derive: install.dos Copy lines, the userland, the staged libraries), including C:Reboot, a file
with no hand-written row before; a dirty IXNET_LIB refuses the start; start() must call it (source check)."""
import hashlib, pathlib, shutil, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, paths

XDF = shutil.which("xdftool")
INSTALL = "Copy vsh C:vsh CLONE\nCopy Reboot C:Reboot CLONE\nCopy up-console.device DEVS:up-console.device CLONE\n" \
          "Copy vtcon-handler L: CLONE\nCopy sz rz UP-Term:bin CLONE\n"

def md5(b):
    return hashlib.md5(b).hexdigest()

@unittest.skipUnless(XDF, "needs xdftool (pip install amitools)")
class SysHdfKit(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.dirty = self.d / "dirty"
        self.dirty.write_bytes(b"x" * 50 + b"AmiTCP:libs/usergroup.library\0" + b"y" * 50)
        self.old = self.d / "old"
        self.old.write_bytes(b"old" * 40)
        (self.d / "build/amiga").mkdir(parents=True)
        (self.d / "dist").mkdir()
        (self.d / "dist/install.dos").write_text(INSTALL)
        (self.d / "userland").mkdir()
        amis = self.d / "vendor/amissl-5.27/AmiSSL/Libs/AmigaOS3"
        for sub in ("AmiSSL/68020-40", "AmiSSL/68060"):
            (amis / sub).mkdir(parents=True)
        files = ["build/amiga/" + n for n in ("vsh", "Reboot", "up-console.device", "vtcon-handler", "sz", "rz")] + \
            ["ixnet", "ixemul", "ixemul-soft", "vendor/amissl-5.27/AmiSSL/Libs/AmigaOS3/amisslmaster.library",
             "vendor/amissl-5.27/AmiSSL/Libs/AmigaOS3/AmiSSL/68020-40/amissl_v362.library",
             "vendor/amissl-5.27/AmiSSL/Libs/AmigaOS3/AmiSSL/68060/amissl_v362.library"]
        for rel in files:
            (self.d / rel).write_bytes(("new " + rel).encode() * 5)
        self.img = self.d / "t.adf"
        old_at = ("Libs/ixemul.library", "Devs/up-console.device", "C/vsh", "AmiSSL/Libs/amisslmaster.library",
                  "AmiSSL/Libs/AmiSSL/amissl_v362.library")
        args = ["create", "+", "format", "T"]
        for dr in ("Libs", "Devs", "C", "AmiSSL", "AmiSSL/Libs", "AmiSSL/Libs/AmiSSL"):
            args += ["+", "makedir", dr]
        args += ["+", "write", str(self.dirty), "Libs/ixnet.library"]
        for dst in old_at:
            args += ["+", "write", str(self.old), dst]
        subprocess.run([XDF, str(self.img), *args], check=True, capture_output=True)   # no L/, no C/Reboot: the image lacks them
        keys = ("IXNET_LIB", "IXEMUL_LIB", "IXEMUL_LIB_SOFT", "VENDOR")
        self.save = ({k: getattr(paths, k) for k in keys}, rig.RIG, rig.ROOT, rig.USERLAND_SRC, rig.M060, rig.NOFPU)
        paths.IXNET_LIB, paths.IXEMUL_LIB, paths.IXEMUL_LIB_SOFT = self.d / "ixnet", self.d / "ixemul", self.d / "ixemul-soft"
        paths.VENDOR = self.d / "vendor"
        rig.RIG = rig.ROOT = self.d
        rig.USERLAND_SRC = self.d / "userland"
        rig.M060, rig.NOFPU = True, False
        def restore():
            for k, v in self.save[0].items():
                setattr(paths, k, v)
            rig.RIG, rig.ROOT, rig.USERLAND_SRC, rig.M060, rig.NOFPU = self.save[1:]
        self.addCleanup(restore)

    def read(self, dst):
        out = self.d / "read"
        subprocess.run([XDF, str(self.img), "read", dst, str(out)], check=True, capture_output=True)
        return out.read_bytes()

    def test_dirty_clone_ends_equal_to_the_derived_set(self):
        rig.sys_hdf_kit(self.img)
        on_disk = [e for e in rig.kit_entries(rig.NOFPU, rig.M060) if e.clone]
        got = {e.dest for e in on_disk}
        for want in ("C:Reboot", "C:vsh", "L:vtcon-handler", "DEVS:up-console.device", "Libs/ixnet.library",
                     "Libs/ixemul.library", "AmiSSL/Libs/amisslmaster.library", "AmiSSL/Libs/AmiSSL/amissl_v362.library"):
            self.assertIn(want, got)
        self.assertNotIn("UP-Term:bin/sz", got)   # not on the system disk: the boot shadows it
        self.assertIn("UP-Term:bin/sz", {x[1] for x in rig.KIT_SHADOWS} | {e.dest for e in rig.kit_entries()})
        for e in on_disk:
            self.assertEqual(md5(self.read(e.clone)), md5(e.src.read_bytes()), e.clone)

    def test_cpu_picks_the_staged_build(self):
        rig.M060, rig.NOFPU = False, True
        rig.sys_hdf_kit(self.img)
        self.assertEqual(self.read("Libs/ixemul.library"), (self.d / "ixemul-soft").read_bytes())
        self.assertEqual(self.read("AmiSSL/Libs/AmiSSL/amissl_v362.library"),
                         (self.d / "vendor/amissl-5.27/AmiSSL/Libs/AmigaOS3/AmiSSL/68020-40/amissl_v362.library").read_bytes())

    def test_dirty_source_refuses(self):
        paths.IXNET_LIB = self.dirty
        with self.assertRaises(SystemExit):
            rig.sys_hdf_kit(self.img)

    def test_start_calls_it(self):
        self.assertIn('sys_hdf_kit(RIG / "sys.hdf")', (ROOT / "tools/rig/rig.py").read_text())

if __name__ == "__main__":
    unittest.main()
