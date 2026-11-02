"""A case that dies with an exception (socket timeout, `rig.py reboot` exiting 1) records a FAIL verdict.

Before, _main caught only SystemExit, so the exception ended the run with no verdict and the results JSON
(fresh-all.json) kept the stale older one (INSTALLER-RIG-TIMEOUT-VERDICT).
"""
import pathlib, socket, subprocess, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import installer_rig as irg


class ExceptionWritesFailVerdict(unittest.TestCase):
    def run_main(self, exc):
        recorded = []
        fx = mock.Mock()
        with mock.patch.object(irg, "CHAIN", False), \
             mock.patch.object(irg.ir, "kit_dir", lambda pkg: ROOT), \
             mock.patch.object(irg, "case_packages", lambda case: ([], None)), \
             mock.patch.object(irg.verdicts, "select", lambda *a: ([("fresh-all", None)], [])), \
             mock.patch.object(irg, "rig", lambda *a: None), \
             mock.patch.object(irg, "rig_start", lambda: None), \
             mock.patch.object(irg.ir, "show_close", lambda: None), \
             mock.patch.object(irg, "run_case", mock.Mock(side_effect=exc)), \
             mock.patch.object(irg, "record", lambda case, ok, detail: recorded.append((case, ok, detail))), \
             mock.patch.object(irg.STORE, "failed", lambda cases: ["fresh-all"]):
            rc = irg._main(fx, ["fresh-all"], set())
        return rc, recorded

    def test_socket_timeout_records_fail(self):
        rc, recorded = self.run_main(socket.timeout("timed out"))
        self.assertEqual(rc, 1)
        self.assertEqual(len(recorded), 1)
        self.assertFalse(recorded[0][1])
        self.assertIn("timed out", recorded[0][2])

    def test_failed_rig_reboot_records_fail(self):
        rc, recorded = self.run_main(subprocess.CalledProcessError(1, ["rig.py", "reboot"]))
        self.assertEqual(rc, 1)
        self.assertFalse(recorded[0][1])
        self.assertIn("CalledProcessError", recorded[0][2])

    def test_system_exit_still_writes_no_verdict(self):
        rc, recorded = self.run_main(SystemExit("rig gone"))
        self.assertEqual(rc, 2)
        self.assertEqual(recorded, [])


if __name__ == "__main__":
    unittest.main()
