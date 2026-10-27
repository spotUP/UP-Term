"""amarchy_menu_rig: one row that aborts fails that row, the table goes on.

open_timing's helper exited with "[ERROR] window 'menuhost' is not the active window and a click did not make it
so" (SystemExit) and the 22 rows after it never ran (rig 7, 2026-10-27, research/2026-10-27-s10-menurig.md).
"""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_menu_rig as m


class RowAbort(unittest.TestCase):
    def test_system_exit_fails_the_row(self):
        def gives_up():
            sys.exit("[ERROR] window 'menuhost' is not the active window")
        ok, detail = m.run_row(gives_up)
        self.assertFalse(ok)
        self.assertIn("menuhost", detail)

    def test_exception_fails_the_row(self):
        ok, detail = m.run_row(lambda: 1 / 0)
        self.assertFalse(ok)
        self.assertIn("aborted", detail)

    def test_row_answer_passes_through(self):
        self.assertEqual(m.run_row(lambda: (True, "fine")), (True, "fine"))


REQ = {"addr": 1, "title": "System Request", "task": "ramlib", "screen": "Workbench",
       "buttons": ["Retry", "Cancel"]}


class RequesterOpen(unittest.TestCase):
    """A ramlib "Please insert volume AmiTCP" requester blocked amarchy-menu and the row only said "text menu did
    not open" (rig 7, 2026-10-27)."""

    def test_open_requester_aborts_row_without_running_it(self):
        ran = []
        ok, detail = m.run_row(lambda: ran.append(1) or (True, "fine"), query=lambda: [REQ])
        self.assertFalse(ok)
        self.assertFalse(ran)
        self.assertIn("system requester open", detail)
        self.assertIn("ramlib", detail)
        self.assertIn("Retry|Cancel", detail)

    def test_timeout_with_requester_names_it(self):
        calls = iter([[], [REQ]])
        ok, detail = m.run_row(lambda: (False, "text menu did not open"), query=lambda: next(calls))
        self.assertFalse(ok)
        self.assertIn("system requester open", detail)
        self.assertIn("text menu did not open", detail)

    def test_no_requester_leaves_verdict(self):
        self.assertEqual(m.run_row(lambda: (False, "x"), query=lambda: []), (False, "x"))


if __name__ == "__main__":
    unittest.main()
