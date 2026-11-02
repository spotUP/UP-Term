#!/usr/bin/env python3
"""RIG-PUSH-STALE-BUILD: rig.py install / refresh_programs make a stale build/amiga program current before pushing it.
Rig 91 ran a pre-K40d handler after a merge because the push copied the old binary as-is. make is faked."""
import pathlib, shutil, subprocess, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class Push(unittest.TestCase):
    def setUp(self):
        self.t = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.t, ignore_errors=True)
        (self.t / "build/amiga").mkdir(parents=True)
        self.vtc = self.t / "vtc"
        self.vtc.mkdir()
        self.bin = self.t / "build/amiga/foo"
        self.bin.write_bytes(b"OLD")
        (self.vtc / "foo").write_bytes(b"RIG")
        for p in (mock.patch.object(rig, "ROOT", self.t), mock.patch.object(rig, "userland_names", return_value=[])):
            p.start()
            self.addCleanup(p.stop)
        self.calls = []

    def fake(self, stale=True, build_rc=0):
        def run(cmd, **kw):
            self.calls.append(cmd)
            if cmd[1] == "-q":
                return subprocess.CompletedProcess(cmd, 1 if stale else 0, "", "")
            if build_rc == 0:
                self.bin.write_bytes(b"NEW")
            return subprocess.CompletedProcess(cmd, build_rc, "", "cc: boom")
        return run

    def test_stale_program_is_made_before_it_lands_on_the_rig(self):
        with mock.patch.object(rig.subprocess, "run", self.fake()):
            rig.refresh_programs(self.vtc, self.t / "build/amiga")
        self.assertEqual((self.vtc / "foo").read_bytes(), b"NEW")
        self.assertIn(["make", "build/amiga/foo"], self.calls)

    def test_current_program_runs_no_build(self):
        with mock.patch.object(rig.subprocess, "run", self.fake(stale=False)):
            rig.refresh_programs(self.vtc, self.t / "build/amiga")
        self.assertEqual([c for c in self.calls if c[1] != "-q"], [])
        self.assertEqual((self.vtc / "foo").read_bytes(), b"OLD")

    def test_failed_build_refuses_naming_the_program(self):
        with mock.patch.object(rig.subprocess, "run", self.fake(build_rc=2)):
            with self.assertRaises(SystemExit) as e:
                rig.refresh_programs(self.vtc, self.t / "build/amiga")
        self.assertIn("[ERROR]", str(e.exception.code))
        self.assertIn("foo", str(e.exception.code))
        self.assertEqual((self.vtc / "foo").read_bytes(), b"RIG")

    def test_install_makes_first(self):
        with mock.patch.object(rig, "ensure_built", side_effect=RuntimeError("sentinel")) as eb:
            with self.assertRaises(RuntimeError):
                rig._install()
        eb.assert_called_once()


if __name__ == "__main__":
    unittest.main()
