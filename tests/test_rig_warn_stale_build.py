#!/usr/bin/env python3
"""rig.py start warns (never blocks) naming the stale build/amiga programs and the newest newer source. make and git faked."""
import os, pathlib, shutil, subprocess, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class Warn(unittest.TestCase):
    def setUp(self):
        self.t = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.t, ignore_errors=True)
        (self.t / "build/amiga").mkdir(parents=True)
        self.bin = self.t / "build/amiga/foo"
        self.bin.write_bytes(b"x")
        os.utime(self.bin, (100, 100))
        for n, m in (("a.c", 200), ("b.h", 300)):
            (self.t / n).write_text("")
            os.utime(self.t / n, (m, m))
        p = mock.patch.object(rig, "ROOT", self.t)
        p.start()
        self.addCleanup(p.stop)

    def fake(self, rc):
        def run(cmd, **kw):
            return subprocess.CompletedProcess(cmd, rc, "a.c\nb.h\n" if cmd[0] == "git" else "", "")
        return run

    def test_stale_warns_with_binary_and_newest_source(self):
        with mock.patch("builtins.print") as pr:
            r = rig.warn_stale_build([self.bin], self.fake(1))
        self.assertEqual(r, ["foo"])
        msg = pr.call_args[0][0]
        self.assertTrue(msg.startswith("[WARN]") and "foo" in msg and "b.h" in msg, msg)

    def test_current_is_silent(self):
        with mock.patch("builtins.print") as pr:
            self.assertEqual(rig.warn_stale_build([self.bin], self.fake(0)), [])
        pr.assert_not_called()


if __name__ == "__main__":
    unittest.main()
