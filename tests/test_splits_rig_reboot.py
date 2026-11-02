#!/usr/bin/env python3
"""splits_rig.reboot: a rig.py start that fails (RIG-STALE-BASE refusal, frozen boot) stops the run with
start's own reason. Before the fix the failure was swallowed and the next case reported a fake HANG
(s51 NS3.3 rerun, ConnectionRefusedError on every --fresh-boot round)."""
import pathlib, subprocess, sys, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import splits_rig


def fake_run(start_rc, out=""):
    def run(cmd, **kw):
        rc = start_rc if "start" in cmd else 0
        return subprocess.CompletedProcess(cmd, rc, stdout=out, stderr="")
    return run


class Reboot(unittest.TestCase):
    def test_failed_start_stops_the_run_with_its_reason(self):
        with mock.patch.object(splits_rig.subprocess, "run", side_effect=fake_run(2, "[ERROR] RIG-STALE-BASE: main lacks origin/main")):
            with self.assertRaises(SystemExit) as e:
                splits_rig.reboot()
        self.assertIn("RIG-STALE-BASE", str(e.exception.code))
        self.assertIn("exit 2", str(e.exception.code))

    def test_good_start_returns(self):
        with mock.patch.object(splits_rig.subprocess, "run", side_effect=fake_run(0)) as run:
            splits_rig.reboot()
        self.assertEqual([c.args[0][-1] if c.args[0][-1] != "--keep" else "stop" for c in run.call_args_list], ["stop", "start"])


if __name__ == "__main__":
    unittest.main()
