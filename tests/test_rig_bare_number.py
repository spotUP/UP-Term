#!/usr/bin/env python3
"""rig.py refuses a bare number after the command: the rig is chosen by UPTERM_RIG only, and `rig.py clear 55` once
ignored the 55 and deleted rig 1 (build/rig). Option values (--os 32) and free-text commands (dbg) stay allowed.
The command functions are mocked: this test never reaches a real clear."""
import pathlib, sys, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class BareNumber(unittest.TestCase):
    def test_refused(self):
        self.assertEqual(rig.bare_number_argument(["rig.py", "clear", "55"]), "55")
        self.assertEqual(rig.bare_number_argument(["rig.py", "start", "--max", "3"]), "3")

    def test_option_values_allowed(self):
        for a in (["start", "--max", "--os", "32"], ["start", "--ram", "128"], ["start", "--purpose", "7"],
                  ["start", "--kick", "40"], ["watchdog", "--idle-minutes", "10"], ["hold", "--hours", "2"], ["clear"]):
            self.assertIsNone(rig.bare_number_argument(["rig.py"] + a), a)

    def test_hold_child_argv_accepted(self):
        # `rig.py hold` spawns `hold --foreground --hours 4`; the guard refused the 4, so the detached hold died at
        # once and the idle watchdog stopped rig 91 during the owner's eye check (2026-11-03).
        with mock.patch.object(rig, "mine", return_value=True), mock.patch.object(rig, "hold_release"), \
                mock.patch.object(rig.subprocess, "Popen") as popen, mock.patch.object(sys, "argv", ["rig.py", "hold"]), \
                mock.patch("sys.stdout"):
            rig.hold()
        child = popen.call_args.args[0]
        self.assertEqual(child[2:5], ["hold", "--foreground", "--hours"])
        self.assertIsNone(rig.bare_number_argument(["rig.py"] + child[2:]), child)

    def test_free_text_commands_allowed(self):
        self.assertIsNone(rig.bare_number_argument(["rig.py", "dbg", "m", "400", "10"]))

    def test_main_exits_before_command(self):
        with mock.patch.object(rig, "clear") as clear, mock.patch.object(rig, "ensure_current"), \
                mock.patch("sys.stderr") as err:
            self.assertEqual(rig.main(["rig.py", "clear", "55"]), 2)
        clear.assert_not_called()
        self.assertIn("UPTERM_RIG=55", "".join(c.args[0] for c in err.write.call_args_list))


if __name__ == "__main__":
    unittest.main()
