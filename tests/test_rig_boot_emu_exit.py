#!/usr/bin/env python3
"""rig.py start's boot wait fails fast when the emulator exited. Process and ps faked."""
import pathlib, sys, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class EmuExit(unittest.TestCase):
    def test_exited_handle_reports_rc(self):
        msg = rig.emu_exit_error(mock.Mock(poll=lambda: 0), True)
        self.assertTrue(msg.startswith("[ERROR] fs-uae exited (rc 0) during boot"), msg)

    def test_running_handle_is_none(self):
        self.assertIsNone(rig.emu_exit_error(mock.Mock(poll=lambda: None), True))

    def test_open_launch_gone_after_seen(self):
        self.assertIn("exited", rig.emu_exit_error(None, True, running=lambda: []))
        self.assertIsNone(rig.emu_exit_error(None, False, running=lambda: []))


if __name__ == "__main__":
    unittest.main()
