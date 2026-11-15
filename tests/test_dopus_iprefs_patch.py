#!/usr/bin/env python3
"""tools/dopus_iprefs_patch.py: one damaged literal before, Latin-1 after, same size, idempotent, [ERROR] otherwise; and on
the real md5-pinned os3 + os3-68000 DirectoryOpus when lha and the vendor archives are there."""
import pathlib, shutil, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import dopus_iprefs_patch as p

PRE, POST = b"\x01\x02sysihack\x00", b"tail"


class T(unittest.TestCase):
    def test_patch(self):
        d = PRE + p.BAD + POST
        n = p.patch(d)
        self.assertEqual(len(n), len(d))
        self.assertEqual(n.count(p.BAD), 0)
        self.assertEqual(n.count(b"\xab IPrefs \xbb\x00"), 1)
        self.assertEqual(p.patch(n), n)

    def test_errors(self):
        for d in (PRE, PRE + p.BAD + p.BAD, p.BAD + p.GOOD):
            with self.assertRaises(ValueError):
                p.patch(d)

    def test_real(self):
        v = subprocess.run(["python3", "-I", str(ROOT / "tools/rig/paths.py"), "vendor"], capture_output=True, text=True).stdout.strip()
        lha = shutil.which("lha")
        for a in ("DOpus5_104_os3.lha", "DOpus5_104_os3-68000.lha"):
            arc = pathlib.Path(v) / "dopus5-5.104" / a
            if not (lha and arc.exists()):
                self.skipTest("no lha or vendor archive")
            with tempfile.TemporaryDirectory() as t:
                subprocess.run([lha, "-xqw=" + t, str(arc), "DOpus5/DirectoryOpus"], check=True)
                b = (pathlib.Path(t) / "DOpus5/DirectoryOpus").read_bytes()
                self.assertEqual(b.count(p.BAD), 1)
                n = p.patch(b)
                self.assertEqual((len(n), n.count(p.BAD), n.count(b"\xab IPrefs \xbb\x00")), (len(b), 0, 1))


if __name__ == "__main__":
    unittest.main()
