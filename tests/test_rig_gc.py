#!/usr/bin/env python3
"""Rig folders clean themselves up (RULES.md rule 7, owner decision 2026-11-02): the sweep (`rig.py gc`, run by `start`)
keeps a fresh folder, a folder with an emulator on it, a held start.lock and rig 1, deletes an idle one and any .del;
`rig.py stop --keep` leaves the folder, `rig.py stop` clears rig N > 1 only. Temp dir, injected process list and clock."""
import contextlib, fcntl, io, os, pathlib, shutil, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

NOW = 1_000_000_000.0
HOUR = 3600


class RigGc(unittest.TestCase):
    def setUp(self):
        self.home = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.home, True)
        st = contextlib.ExitStack()
        self.addCleanup(st.close)
        st.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def folder(self, name, age_hours):
        d = self.home / name
        d.mkdir()
        (d / "sys.hdf").write_bytes(b"x" * 10)
        (d / "last-command").write_text("%f\n" % (NOW - age_hours * HOUR))
        return d

    def sweep(self, lines=(), own_n=9):
        return rig.gc(home=self.home, own_n=own_n, lines=list(lines), now=NOW)

    def test_decision_table(self):
        d = rig.gc_decision
        self.assertFalse(d(2, False, False, NOW - HOUR, NOW)[0])          # fresh
        self.assertTrue(d(2, False, False, NOW - 3 * HOUR, NOW)[0])       # stale, no process
        self.assertFalse(d(2, True, False, NOW - 9 * HOUR, NOW)[0])       # process
        self.assertFalse(d(2, False, True, NOW - 9 * HOUR, NOW)[0])       # start in progress
        self.assertFalse(d(1, False, False, NOW - 99 * HOUR, NOW)[0])     # rig 1

    def test_sweep_keeps_and_deletes(self):
        self.folder("rig", 99)
        self.folder("rig2", 1)
        self.folder("rig3", 5)
        self.folder("rig4", 5)
        self.folder("rig9", 99)   # own number
        self.folder("rig5", 5)
        (self.home / "rig6.del").mkdir()
        (self.home / "rig6.del" / "f").write_bytes(b"y")
        lines = ["123 /x/fs-uae %s/rig4/vtcon-rig.fs-uae" % self.home]
        with open(self.home / "rig5" / "start.lock", "w") as held:
            fcntl.flock(held, fcntl.LOCK_EX)
            removed = self.sweep(lines)
        self.assertEqual(sorted(removed), ["rig3", "rig6.del"])
        for kept in ("rig", "rig2", "rig4", "rig5", "rig9"):
            self.assertTrue((self.home / kept).is_dir(), kept)
        self.assertFalse((self.home / "rig3").exists())
        log = (self.home / "rig-gc.log").read_text()
        self.assertIn("rig3", log)
        self.assertIn("rig6.del", log)

    def test_process_on_rig2_does_not_protect_rig21(self):
        self.folder("rig2", 5)
        self.folder("rig21", 5)
        removed = self.sweep(["1 fs-uae %s/rig2/vtcon-rig.fs-uae" % self.home])
        self.assertEqual(removed, ["rig21"])

    def test_remove_failure_is_reported_not_silent(self):
        d = self.folder("rig3", 5)
        with mock.patch.object(rig.shutil, "rmtree", side_effect=OSError("busy")), \
             mock.patch.object(rig.time, "sleep", lambda s: None), \
             contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertFalse(rig.remove_folder(d))
        self.assertIn("[ERROR]", err.getvalue())


class RigStopClears(unittest.TestCase):
    def run_stop(self, argv, rig_n, running=False):
        calls = []
        with mock.patch.object(rig, "stop", lambda: calls.append("stop")), \
             mock.patch.object(rig, "clear", lambda: calls.append("clear")), \
             mock.patch.object(rig, "mine", lambda: ["pid"] if running else []), \
             mock.patch.object(rig.paths, "RIG_N", rig_n), mock.patch.object(rig.sys, "argv", argv):
            rig.main(argv)
        return calls

    def test_stop_clears_rig_above_one_only_without_keep(self):
        self.assertEqual(self.run_stop(["rig.py", "stop"], 3), ["stop", "clear"])
        self.assertEqual(self.run_stop(["rig.py", "stop", "--keep"], 3), ["stop"])
        self.assertEqual(self.run_stop(["rig.py", "stop"], 1), ["stop"])
        self.assertEqual(self.run_stop(["rig.py", "stop"], 3, running=True), ["stop"])



class ClearRefusesRigOne(unittest.TestCase):
    """`rig.py clear` with UPTERM_RIG unset targets rig 1 (the owner's); it must refuse unless --rig1 is given."""
    def run_clear(self, argv, n):
        removed = []
        with mock.patch.object(rig.paths, "RIG_N", n), mock.patch.object(sys, "argv", argv), \
             mock.patch.object(rig, "mine", lambda: False), \
             mock.patch.object(rig, "RIG", pathlib.Path(tempfile.mkdtemp())), \
             mock.patch.object(rig, "remove_folder", lambda f: removed.append(f) or True), \
             contextlib.redirect_stdout(io.StringIO()):
            try:
                rig.clear()
            except SystemExit as e:
                return removed, str(e)
        return removed, None

    def test_rig_one_refused(self):
        removed, err = self.run_clear(["rig.py", "clear"], 1)
        self.assertEqual(removed, [])
        self.assertIn("owner's", err)

    def test_rig_one_with_flag_cleared(self):
        self.assertEqual(len(self.run_clear(["rig.py", "clear", "--rig1"], 1)[0]), 1)

    def test_other_rig_cleared(self):
        self.assertEqual(len(self.run_clear(["rig.py", "clear"], 7)[0]), 1)


if __name__ == "__main__":
    unittest.main()
