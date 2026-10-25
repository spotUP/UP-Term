"""installer_rig's command line and the bigvol precondition, on the host (no rig).

--only used to be ignored without a word, and the run went on with every case
(2026-10-25). bigvol saw the UP-Term: assign rig.py install leaves, so the
Installer offered that drawer instead of the biggest volume.
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import installer_rig as irg


class ParseArgs(unittest.TestCase):
    def test_dash_only_selects_the_case(self):
        self.assertEqual(irg.parse_args(["--only", "bigvol"]), (["bigvol"], set()))

    def test_amiga_only_selects_the_case(self):
        self.assertEqual(irg.parse_args(["ONLY", "dest"]), (["dest"], set()))

    def test_flags(self):
        self.assertEqual(irg.parse_args(["FAILED"]), ([], {"FAILED"}))
        self.assertEqual(irg.parse_args([]), ([], set()))

    def test_unknown_word_stops(self):
        with self.assertRaises(SystemExit):
            irg.parse_args(["--onyl", "bigvol"])

    def test_unknown_case_stops(self):
        with self.assertRaises(SystemExit):
            irg.parse_args(["ONLY", "nosuchcase"])

    def test_only_without_case_stops(self):
        with self.assertRaises(SystemExit):
            irg.parse_args(["--only"])


class BigvolPrecondition(unittest.TestCase):
    def test_upterm_assign_removed_before_the_installer(self):
        calls = []
        def run(cmd, *a, **k):
            calls.append(cmd)
            return 0, ""
        def find_installer():
            calls.append("find_installer")
            return None   # stop the case before it drives the Installer
        with mock.patch.object(irg.ir, "prepare"), mock.patch.object(irg.ir, "run", run), \
                mock.patch.object(irg, "find_installer", find_installer):
            irg.run_bigvol()
        self.assertIn("Assign >NIL: UP-Term:", calls)
        self.assertLess(calls.index("Assign >NIL: UP-Term:"), calls.index("find_installer"))


if __name__ == "__main__":
    unittest.main()
