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

    def test_max_is_a_noop_and_purpose_reaches_every_rig_start(self):
        self.addCleanup(irg.PURPOSE.clear)
        self.assertEqual(irg.parse_args(["ONLY", "dest", "--purpose", "vtcon-05: check", "--max"]), (["dest"], set()))
        with mock.patch.object(irg, "rig") as rig:
            irg.rig_start()
        rig.assert_called_once_with("start", "--purpose", "vtcon-05: check")

    def test_no_dopus_reaches_the_rig_start_before_or_after_purpose(self):
        # --no-dopus before --purpose was wiped when set_purpose replaced the list it shared
        for argv in (["--no-dopus", "--purpose", "vtcon-05: d9"], ["--purpose", "vtcon-05: d9", "--no-dopus"]):
            self.addCleanup(irg.PURPOSE.clear)
            self.addCleanup(irg.RIG_FLAGS.clear)
            irg.PURPOSE.clear()
            irg.RIG_FLAGS.clear()
            irg.parse_args(argv)
            with mock.patch.object(irg, "rig") as rig:
                irg.rig_start()
            rig.assert_called_once_with("start", "--purpose", "vtcon-05: d9", "--no-dopus")

    def test_a500_reaches_the_rig_start(self):
        # D10: the 68000 half of the dopus case needs a 68000 rig; --a500 used to stop as an unknown argument
        self.addCleanup(irg.PURPOSE.clear)
        self.addCleanup(irg.RIG_FLAGS.clear)
        irg.parse_args(["ONLY", "addon-dopus", "--no-dopus", "--a500", "--purpose", "vtcon-05: d10"])
        with mock.patch.object(irg, "rig") as rig:
            irg.rig_start()
        rig.assert_called_once_with("start", "--purpose", "vtcon-05: d10", "--no-dopus", "--a500")

    def test_purpose_without_text_stops(self):
        with self.assertRaises(SystemExit):
            irg.parse_args(["--purpose"])

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
