#!/usr/bin/env python3
"""User files on a collected rig survive (RIG-GC-SAVE-USER-FILES, RULES.md rule 7). The sweep that `rig.py start` runs
removed the idle build/rig3 of another session and with it the DCTelnet.Prefs and DCTelnet.Book the owner had made on
that rig, the only copies. Now, before the removal, every file written on the rig (missing from the clone source or
different from it) is copied to build/rig-saved/<n>/<date-time>/; kit files the rig did not change are not; a failed
save keeps the folder. Temp dir, injected process list and clock; no real rig is touched."""
import contextlib, io, os, pathlib, shutil, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

NOW = 1_000_000_000.0
HOUR = 3600


class UserFilesOnACollectedRigSurvive(unittest.TestCase):
    def setUp(self):
        self.home = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.home, True)
        self.out, self.err = io.StringIO(), io.StringIO()
        st = contextlib.ExitStack()
        self.addCleanup(st.close)
        st.enter_context(contextlib.redirect_stdout(self.out))
        st.enter_context(contextlib.redirect_stderr(self.err))
        # rig 1's VTC: drawer, the clone source of every other rig's VTC: (seed_rig_vtc)
        src = self.home / "rig/vtc"
        (src / "C").mkdir(parents=True)
        (src / "C/vsh").write_bytes(b"kit vsh " * 100)
        (src / "C/vsh.uaem").write_bytes(b"----rwed\n")
        (src / "s").mkdir()
        (src / "s/vsh.rc").write_bytes(b"kit rc\n")

    def make_rig(self, n, user_name="DCTelnet.Book", user_bytes=b"BBS phonebook\x00\x01\x02"):
        d = self.home / ("rig%d" % n)
        d.mkdir()
        rig.seed_rig_vtc(self.home / "rig/vtc", d / "vtc")   # the real seeding: a copy of rig 1's drawer
        (d / "fs-uae.log").write_bytes(b"x" * 10)   # no sys.hdf: a junk image fails its unpack (test_rig_gc_save_sys_hdf)
        (d / "last-command").write_text("%f\n" % (NOW - 5 * HOUR))
        later = time_after(d / "vtc/s/vsh.rc")
        # written on the rig: a new file in a new drawer, and a kit file the user changed
        (d / "vtc/DCTelnet").mkdir()
        (d / "vtc/DCTelnet" / user_name).write_bytes(user_bytes)
        (d / "vtc/s/vsh.rc").write_bytes(b"user rc, same length\n")
        os.utime(d / "vtc/s/vsh.rc", (later, later))
        # Work: has no clone source: everything newer than the drive is the user's
        (d / "work").mkdir()
        (d / "work/DCTelnet.Prefs").write_bytes(b"prefs")
        os.utime(d / "work/DCTelnet.Prefs", (later + 10, later + 10))
        return d

    def sweep(self):
        return rig.gc(home=self.home, own_n=9, lines=[], now=NOW)

    def saves(self, n):
        base = self.home / "rig-saved" / str(n)
        return sorted(base.iterdir()) if base.is_dir() else []

    def test_user_files_saved_kit_files_not_folder_removed(self):
        self.make_rig(3)
        self.assertEqual(self.sweep(), ["rig3"])
        self.assertFalse((self.home / "rig3").exists())
        saves = self.saves(3)
        self.assertEqual(len(saves), 1, "no save folder under build/rig-saved/3")
        got = sorted(str(p.relative_to(saves[0])) for p in saves[0].rglob("*") if p.is_file())
        self.assertEqual(got, ["vtc/DCTelnet/DCTelnet.Book", "vtc/s/vsh.rc", "work/DCTelnet.Prefs"])
        self.assertEqual((saves[0] / "vtc/DCTelnet/DCTelnet.Book").read_bytes(), b"BBS phonebook\x00\x01\x02")
        self.assertEqual((saves[0] / "vtc/s/vsh.rc").read_bytes(), b"user rc, same length\n")
        self.assertEqual((saves[0] / "work/DCTelnet.Prefs").read_bytes(), b"prefs")
        self.assertIn("[INFO] rig gc: rig 3: saved 3 files", self.out.getvalue())
        self.assertIn("3 files", (self.home / "rig-gc.log").read_text())

    def test_second_collection_does_not_overwrite_first(self):
        self.make_rig(3)
        self.sweep()
        self.make_rig(3, user_bytes=b"second phonebook")
        self.sweep()
        saves = self.saves(3)
        self.assertEqual(len(saves), 2)
        self.assertEqual((saves[0] / "vtc/DCTelnet/DCTelnet.Book").read_bytes(), b"BBS phonebook\x00\x01\x02")
        self.assertEqual((saves[1] / "vtc/DCTelnet/DCTelnet.Book").read_bytes(), b"second phonebook")

    def test_failed_save_keeps_the_folder(self):
        self.make_rig(3)
        (self.home / "rig-saved").write_bytes(b"not a directory")   # every save destination fails
        self.assertEqual(self.sweep(), [])
        self.assertTrue((self.home / "rig3/vtc/DCTelnet/DCTelnet.Book").is_file())
        self.assertIn("[ERROR] rig gc: could not save", self.err.getvalue())

    def test_rig_with_nothing_written_is_removed_without_a_save(self):
        d = self.home / "rig4"
        d.mkdir()
        rig.seed_rig_vtc(self.home / "rig/vtc", d / "vtc")
        (d / "last-command").write_text("%f\n" % (NOW - 5 * HOUR))
        self.assertEqual(self.sweep(), ["rig4"])
        self.assertEqual(self.saves(4), [])


def time_after(p):
    """A modification time clearly after p's (a rig write comes after the seeding copy)."""
    return p.stat().st_mtime + 100


if __name__ == "__main__":
    unittest.main()
