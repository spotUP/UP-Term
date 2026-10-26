"""Every rig start strips the kit's wasabi block from its fresh sys.hdf clone.

The master image's S:User-Startup carries ';BEGIN UP-Term wasabi' (Run >NIL:
C:wasabid). That wasabid starts before BOOTX:go sets wasabi.key, serves an
empty key on port 1234 (the Mac's port: FS-UAE's bsdsocket shares them), and
took the port from rig 1's BOOTX:wasabid: every request failed with
'auth failed: bad key' and rig.py start gave up after three boots (2026-10-25).
"""
import pathlib, shutil, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig

XDF = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
USER_STARTUP = (b"; user lines\n"
                b";BEGIN UP-Term wasabi\nRun >NIL: C:wasabid\n;END UP-Term wasabi\n"
                b";BEGIN UP-Term assign\nAssign UP-Term: Work:UP-Term\n;END UP-Term assign\n")
USER_STARTUP_DOPUS = (b"; user lines\n;BEGIN DirectoryOpus 5\nASSIGN DOpus5: \"dh0:Opus5\"\nPATH DOpus5:c Add\n;END DirectoryOpus 5\n"
                      b";BEGIN UP-Term assign\nAssign UP-Term: Work:UP-Term\n;END UP-Term assign\n")


@unittest.skipUnless(pathlib.Path(XDF).exists(), "needs amitools' xdftool")
class SysHdfNoWasabid(unittest.TestCase):
    def test_the_wasabi_block_leaves_the_clone_and_the_rest_stays(self):
        saved = rig.RIG
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            image = d / "sys.hdf"
            subprocess.run([XDF, str(image), "create", "size=1M", "+", "format", "System", "ffs"], check=True, capture_output=True)
            subprocess.run([XDF, str(image), "makedir", "S"], check=True, capture_output=True)
            (d / "us").write_bytes(USER_STARTUP)
            subprocess.run([XDF, str(image), "write", str(d / "us"), "S/User-Startup"], check=True, capture_output=True)
            try:
                rig.RIG = d
                rig.sys_hdf_no_wasabid(image)
            finally:
                rig.RIG = saved
            subprocess.run([XDF, str(image), "read", "S/User-Startup", str(d / "out")], check=True, capture_output=True)
            out = (d / "out").read_bytes()
        self.assertNotIn(b"C:wasabid", out, "the clone still starts a keyless C:wasabid on port 1234")
        self.assertIn(b";BEGIN UP-Term assign", out, "only the wasabi block goes")
        self.assertTrue(out.startswith(b"; user lines\n"))

    def test_start_strips_the_clone(self):
        src = (ROOT / "tools/rig/rig.py").read_text()
        body = src[src.index("def start(retries"):]
        body = body[:body.index("\ndef ")]
        self.assertIn('sys_hdf_no_wasabid(RIG / "sys.hdf")', body, "start() no longer strips the fresh clone")

@unittest.skipUnless(pathlib.Path(XDF).exists(), "needs amitools' xdftool")
class SysHdfNoDopus(unittest.TestCase):
    def test_no_dopus_drops_the_assign_block_and_removes_opus5(self):
        saved = rig.RIG
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            image = d / "sys.hdf"
            x = lambda *a: subprocess.run([XDF, str(image), *a], check=True, capture_output=True, text=True)
            x("create", "size=1M", "+", "format", "System", "ffs")
            for n in ("S", "C", "Libs", "Opus5"):
                x("makedir", n)
            for n, dst in (("us", "S/User-Startup"), ("lw", "C/loadwb_old"), ("lw", "C/LoadWB"),
                           ("lw", "Libs/workbench.library"), ("lw", "Opus5/DirectoryOpus")):
                (d / n).write_bytes(USER_STARTUP_DOPUS if n == "us" else b"x")
                x("write", str(d / n), dst)
            (d / "i").write_bytes(b"i")
            x("write", str(d / "i"), "Opus5.info")
            try:
                rig.RIG = d
                rig.sys_hdf_no_dopus(image)
            finally:
                rig.RIG = saved
            x("read", "S/User-Startup", str(d / "out"))
            out = (d / "out").read_bytes()
            top = [l.split()[0] for l in x("list").stdout.splitlines() if l.strip()]
        self.assertNotIn(b"DOpus5:", out, "the clone still assigns DOpus5: (install.dos takes its already-installed branch)")
        self.assertIn(b"; user lines\n", out)
        self.assertIn(b";BEGIN UP-Term assign", out, "only the DirectoryOpus 5 block goes")
        self.assertNotIn("Opus5", top)
        self.assertNotIn("Opus5.info", top)


if __name__ == "__main__":
    unittest.main()
