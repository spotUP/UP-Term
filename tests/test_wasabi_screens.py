#!/usr/bin/env python3
"""`rig.py wasabi screens` lists the screens through the client's `windows`; bare `wasabi` prints usage on stdout;
an unreachable wasabid gives an [ERROR] line (agents saw nothing: `screens` was unknown, usage went to stderr)."""
import contextlib, io, pathlib, subprocess, sys, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import wasabi


def done(rc, out="", err=""):
    return subprocess.CompletedProcess([], rc, out, err)


class Screens(unittest.TestCase):
    def test_screens_prints_client_windows(self):
        buf = io.StringIO()
        with mock.patch.object(wasabi.subprocess, "run", return_value=done(0, 'screen "Workbench"\n')) as run, \
                contextlib.redirect_stdout(buf):
            wasabi.main(["screens"])
        self.assertEqual(buf.getvalue(), 'screen "Workbench"\n')
        self.assertEqual(run.call_args[0][0][-1], "windows")

    def test_unreachable_is_an_error_line(self):
        with mock.patch.object(wasabi.subprocess, "run", return_value=done(1, "", "connection refused")):
            with self.assertRaises(SystemExit) as c:
                wasabi.main(["screens"])
        self.assertTrue(str(c.exception).startswith("[ERROR]"))

    def test_timeout_is_an_error_line(self):
        with mock.patch.object(wasabi.subprocess, "run", side_effect=subprocess.TimeoutExpired("x", 30)):
            with self.assertRaises(SystemExit) as c:
                wasabi.main(["screens"])
        self.assertTrue(str(c.exception).startswith("[ERROR]"))

    def test_bare_prints_usage_on_stdout(self):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            wasabi.main([])
        self.assertIn("screens", buf.getvalue())


if __name__ == "__main__":
    unittest.main()
