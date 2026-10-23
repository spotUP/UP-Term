#!/usr/bin/env python3
"""rig.main: --help, -h, help and ? after any subcommand print usage and run nothing (`rig.py stop --help` once
really stopped the rig). Drives the real entry point; the command functions are replaced by sentinels."""
import contextlib, io, pathlib, sys, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

CMDS = ("setup", "start", "stop", "install", "kit_restore", "status", "logwatch", "muforce", "wasabi", "screenmode")

class RigHelp(unittest.TestCase):
    def run_main(self, *args):
        ran = []
        with contextlib.ExitStack() as st:
            for c in CMDS:
                st.enter_context(mock.patch.object(rig, c, lambda *a, _c=c: ran.append(_c)))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = rig.main(["rig.py", *args])
        return rc, out.getvalue(), ran

    def test_help_forms_after_every_command_run_nothing(self):
        for cmd in ("setup", "start", "stop", "install", "kit-restore", "status", "logwatch", "muforce", "wasabi", "aga", "aga256", "rtg"):
            for h in ("--help", "-h", "help", "?"):
                rc, out, ran = self.run_main(cmd, h)
                self.assertEqual((rc, ran), (0, []), (cmd, h))
                self.assertIn("rig.py stop", out, (cmd, h))

    def test_stop_runs_without_help(self):
        rc, out, ran = self.run_main("stop")
        self.assertEqual((rc, ran), (0, ["stop"]))

if __name__ == "__main__":
    unittest.main()
