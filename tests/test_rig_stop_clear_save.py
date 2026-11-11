#!/usr/bin/env python3
"""`rig.py stop` and `rig.py clear` save the files written on the rig before they remove its folder
(RIG-GC-SAVE-USER-FILES part 2, RULES.md rule 7). Part 1 saved them only in the gc sweep; stop (rig N > 1) and clear
removed build/rig<n> with the user's files in it. Now every removal goes through one step, save_and_remove: drive
folders and the sys.hdf image are copied to build/rig-saved/<n>/<date-time>/ first, and a failed save keeps the folder.
Temp dir, small RDB image, stop()'s emulator kill replaced by a no-op; no real rig touched."""
import contextlib, io, pathlib, shutil, sys, tempfile, unittest, unittest.mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
sys.path.insert(0, str(ROOT / "tests"))
import rig
import paths
from test_rig_gc_save_sys_hdf import MASTER_FILES, make_image, write_into

USER = {"vtc/DCTelnet/DCTelnet.Book": b"BBS phonebook\x00\x01",
        "sys/Prefs/Env-Archive/DCTelnet.prefs": b"user prefs on DH0"}


class StopAndClearSaveFirst(unittest.TestCase):
    def setUp(self):
        self.home = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.home, True)
        self.out, self.err = io.StringIO(), io.StringIO()
        st = contextlib.ExitStack()
        self.addCleanup(st.close)
        st.enter_context(contextlib.redirect_stdout(self.out))
        st.enter_context(contextlib.redirect_stderr(self.err))
        master = self.home / "master.hdf"
        make_image(master, MASTER_FILES)
        src = self.home / "rig/vtc"   # rig 1's VTC:, the clone source
        (src / "C").mkdir(parents=True)
        (src / "C/vsh").write_bytes(b"kit vsh")
        self.rd = self.home / "rig9"
        self.rd.mkdir()
        rig.seed_rig_vtc(src, self.rd / "vtc")
        (self.rd / "vtc/DCTelnet").mkdir()
        (self.rd / "vtc/DCTelnet/DCTelnet.Book").write_bytes(USER["vtc/DCTelnet/DCTelnet.Book"])
        shutil.copyfile(master, self.rd / "sys.hdf")
        write_into(self.rd / "sys.hdf", {"Prefs/Env-Archive/DCTelnet.prefs": USER["sys/Prefs/Env-Archive/DCTelnet.prefs"]})
        for target, name, value in ((rig, "RIG", self.rd), (paths, "RIG_N", 9), (rig, "SRC_HDF", master),
                                    (rig, "mine", lambda *a: []), (rig, "stop", lambda *a: None),
                                    (rig, "hold_release", lambda *a: None), (rig.rigwasabi, "stop_stream", lambda *a: None),
                                    (sys, "argv", ["rig.py", "stop"])):
            st.enter_context(unittest.mock.patch.object(target, name, value))

    def saved(self):
        base = self.home / "rig-saved/9"
        saves = sorted(base.iterdir()) if base.is_dir() else []
        self.assertEqual(len(saves), 1, "no save folder under rig-saved/9: the folder was removed unsaved")
        return {str(p.relative_to(saves[0])): p.read_bytes() for p in saves[0].rglob("*") if p.is_file()}

    def test_clear_saves_then_removes(self):
        rig.clear()
        self.assertFalse(self.rd.exists())
        self.assertEqual(self.saved(), USER)
        self.assertIn("[INFO] rig clear: rig 9: saved 2 files", self.out.getvalue())

    def test_stop_saves_then_removes(self):
        rig.stop_command()
        self.assertFalse(self.rd.exists())
        self.assertEqual(self.saved(), USER)

    def test_failed_save_keeps_the_folder(self):
        (self.home / "rig-saved").write_bytes(b"not a directory")   # every save destination fails
        with self.assertRaises(SystemExit):
            rig.clear()
        self.assertTrue((self.rd / "vtc/DCTelnet/DCTelnet.Book").is_file())
        self.assertIn("[ERROR] rig clear: could not save", self.err.getvalue())

    def test_one_shared_step_for_every_removal(self):
        calls = []
        real = rig.save_and_remove
        with unittest.mock.patch.object(rig, "save_and_remove", lambda *a, **k: calls.append(a[0]) or real(*a, **k)):
            rig.clear()
            self.rd.mkdir()
            (self.rd / "last-command").write_text("1\n")
            rig.gc(home=self.home, own_n=2, lines=[], now=10_000_000_000.0)
        self.assertEqual(calls, [self.rd, self.rd])


if __name__ == "__main__":
    unittest.main()
