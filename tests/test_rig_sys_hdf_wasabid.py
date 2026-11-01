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


@unittest.skipUnless(pathlib.Path(XDF).exists(), "needs amitools' xdftool")
class SysHdfDopus(unittest.TestCase):
    """The clone edit that gives the rig Directory Opus 5.104 (recorded by `rig.py dopus`) in place of the master's 5.82."""
    BLOCK = b";BEGIN DOpus5\nAssign DOpus5: \"VTCX:DOpus5\"\nPath DOpus5:C ADD\nResident LoadWB DOpus5:C/LoadDB PURE\n;END DOpus5\n"

    def run_edit(self, record):
        saved = rig.RIG
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            image = d / "sys.hdf"
            x = lambda *a: subprocess.run([XDF, str(image), *a], check=True, capture_output=True, text=True)
            x("create", "size=1M", "+", "format", "System", "ffs")
            for n in ("S", "C", "Libs", "Opus5", "Prefs", "Prefs/Env-Archive"):
                x("makedir", n)
            files = (("us", "S/User-Startup", USER_STARTUP_DOPUS), ("old", "C/loadwb_old", b"stock LoadWB"),
                     ("new", "C/LoadWB", b"LoadDB 62.0"), ("wb", "Libs/workbench.library", b"wb45"),
                     ("op", "Opus5/DirectoryOpus", b"5.82"), ("i", "Opus5.info", b"i"))
            for n, dst, data in files:
                (d / n).write_bytes(data)
                x("write", str(d / n), dst)
            vtc = d / "vtc"
            (vtc / "DOpus5").mkdir(parents=True)
            if record:
                (vtc / "DOpus5/DirectoryOpus").write_bytes(b"5.104")
                (vtc / "DOpus5/C").mkdir()
                (vtc / "DOpus5/C/LoadDB").write_bytes(b"LoadDB 5.104")
                (vtc / rig.DOPUS_BLOCK).write_bytes(self.BLOCK)
                (vtc / rig.DOPUS_STAMP).write_text(rig.dopus_archive_md5() + "\n")
                (vtc / rig.DOPUS_ENVARC).mkdir()
                (vtc / rig.DOPUS_ENVARC / "DOpus5Startup").write_bytes(b"workbench")
            try:
                rig.RIG = d
                rig.sys_hdf_dopus(image)
            finally:
                rig.RIG = saved
            out = {}
            for src in ("S/User-Startup", "C/LoadWB", "Libs/workbench.library", "Prefs/Env-Archive/DOpus5Startup"):
                r = subprocess.run([XDF, str(image), "read", src, str(d / "o")], capture_output=True)
                out[src] = (d / "o").read_bytes() if r.returncode == 0 else None
                (d / "o").unlink(missing_ok=True)
            out["top"] = [l.split()[0] for l in x("list").stdout.splitlines() if l.strip()]
            return out

    def test_the_recorded_5104_block_replaces_5_82(self):
        out = self.run_edit(True)
        us = out["S/User-Startup"]
        self.assertNotIn(b"dh0:Opus5", us, "the 5.82 block is still in the clone")
        self.assertNotIn(b"BEGIN DirectoryOpus 5", us)
        self.assertIn(self.BLOCK, us, "the recorded 5.104 block is missing")
        self.assertIn(b";BEGIN UP-Term assign", us, "only the 5.82 block goes")
        self.assertEqual(out["C/LoadWB"], b"LoadDB 5.104", "C/LoadWB is not 5.104's LoadDB (Startup-Sequence calls C:LoadWB by path)")
        self.assertEqual(out["Libs/workbench.library"], b"wb45", "Libs/workbench.library stays")
        self.assertEqual(out["Prefs/Env-Archive/DOpus5Startup"], b"workbench", "the recorded ENVARC file is not back")
        self.assertNotIn("Opus5", out["top"])
        self.assertNotIn("Opus5.info", out["top"])

    def test_no_stamp_keeps_5_82_and_warns(self):
        import contextlib, io
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            out = self.run_edit(False)
        self.assertIn("[WARN]", buf.getvalue())
        self.assertIn("rig.py dopus", buf.getvalue())
        self.assertEqual(out["S/User-Startup"], USER_STARTUP_DOPUS)
        self.assertEqual(out["C/LoadWB"], b"LoadDB 62.0")
        self.assertIn("Opus5", out["top"])

    def test_start_calls_the_edit_unless_no_dopus(self):
        body = (ROOT / "tools/rig/rig.py").read_text()
        self.assertIn('else:\n            sys_hdf_dopus(RIG / "sys.hdf")', body)


if __name__ == "__main__":
    unittest.main()
