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


class Addon68000(unittest.TestCase):
    """D10 (RIG-A500-KIT-BOOT, 2026-10-29): on the 68000 rig the dopus case ran the base Installer first; the base
    refuses the 68000 by design, so the case failed on "a system requester came up" before the add-on ran."""

    def run_case(self, flags):
        self.addCleanup(irg.RIG_FLAGS.clear)
        irg.RIG_FLAGS[:] = flags
        kits = []
        def installer_run(case, inst, kitdir, *a, **k):
            kits.append(kitdir)
            return False, "", None   # stop the case after its first Installer
        with mock.patch.object(irg, "ir") as ir, mock.patch.object(irg, "read_installer", return_value=(None, None)), \
                mock.patch.object(irg, "installer_run", installer_run), \
                mock.patch.object(irg, "installer_done", return_value=False):
            ir.standalone_ids.return_value = ["dopus"]
            ir.run.return_value = (5, "")
            ir.run_long.return_value = (0, "")
            irg.run_addon_case("dopus")
        return kits

    def test_68000_rig_runs_the_dopus_install_without_the_base(self):
        self.assertEqual(self.run_case(["--a500"]), ["kit-dopus"])

    def test_68020_rig_still_runs_the_base_first(self):
        self.assertEqual(self.run_case([]), ["distkit"])


if __name__ == "__main__":
    unittest.main()
