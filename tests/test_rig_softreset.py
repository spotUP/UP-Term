#!/usr/bin/env python3
"""rig.py softreset: SIGUSR2 only to a patched fs-uae, refuses (non-zero) an unpatched one. ps, os.kill faked."""
import pathlib, shutil, signal, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class Softreset(unittest.TestCase):
    def setUp(self):
        self.t = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.t, ignore_errors=True)

    def binary(self, patched):
        b = self.t / ("fs-uae-patched" if patched else "fs-uae-stock")
        b.write_bytes(b"\x00ELF" + (rig.SOFTRESET_MARKER if patched else b"") + b"\x00")
        return b

    def run_cmd(self, b):
        with mock.patch.object(rig, "mine", return_value=["4242 %s --window-title=x" % b]), mock.patch("os.kill") as kill:
            try:
                rig.softreset()
                rc = 0
            except SystemExit as e:
                rc = e.code
        return rc, kill

    def test_patched_gets_sigusr2(self):
        rc, kill = self.run_cmd(self.binary(True))
        self.assertEqual(rc, 0)
        kill.assert_called_once_with(4242, signal.SIGUSR2)

    def test_unpatched_refused_without_signal(self):
        rc, kill = self.run_cmd(self.binary(False))
        self.assertIn("without the soft reset patch", rc)
        kill.assert_not_called()

    def test_no_rig_running(self):
        with mock.patch.object(rig, "mine", return_value=[]):
            with self.assertRaises(SystemExit) as e:
                rig.softreset()
        self.assertIn("no FS-UAE running", e.exception.code)


if __name__ == "__main__":
    unittest.main()
