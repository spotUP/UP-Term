"""rig.py reboot --emu <other> stops the emulator the rig RUNS (recorded in <rig>/emu), not the one --emu names for the next start.

2026-11-09: rig 3 ran fs-uae; `reboot --emu amiberry` pkill'ed the Amiberry key, printed "stopped", fs-uae stayed up
and the start refused on the wasabi port.
"""
import pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import paths
import rig

FS_KEY = str(rig.RIG / "vtcon-rig.fs-uae")
AMI_KEY = str(rig.RIG / "win") + "/"
PS = "  43606 /usr/bin/fs-uae %s --wasabi\n  500 /usr/bin/other\n" % FS_KEY


def fake_run(calls, ps_out=PS, alive=[True]):
    def run(cmd, *a, **k):
        calls.append(list(cmd))
        r = mock.Mock(stdout=ps_out if alive[0] else "")
        if cmd[0] == "pkill":
            alive[0] = False
        return r
    return run


class RebootEmuSwitch(unittest.TestCase):
    def test_mine_finds_the_named_emulator_whatever_paths_emu_is(self):
        with mock.patch.object(rig.subprocess, "run", fake_run([])):
            self.assertEqual(len(rig.mine("fsuae")), 1)
            self.assertEqual(rig.mine("amiberry"), [])

    def test_reboot_emu_amiberry_stops_the_recorded_fsuae(self):
        calls = []
        with tempfile.TemporaryDirectory() as d:
            emu_file = pathlib.Path(d) / "emu"
            emu_file.write_text("fsuae\n")
            with mock.patch.object(paths, "EMU_FILE", emu_file), mock.patch.object(paths, "EMU", "amiberry"), \
                 mock.patch.object(rig, "AMIBERRY", True), mock.patch.object(rig, "EMU_KEY", AMI_KEY), \
                 mock.patch.object(rig, "status", return_value=False), mock.patch.object(rig.time, "sleep"), \
                 mock.patch.object(rig, "reboot_args", return_value=["--emu", "amiberry"]), \
                 mock.patch.object(rig.subprocess, "run", fake_run(calls)):
                rig.reboot([])
        pkills = [c for c in calls if c[0] == "pkill"]
        self.assertTrue(pkills, "no pkill sent")
        self.assertEqual(pkills[0][-1], FS_KEY)
        self.assertNotIn(AMI_KEY, [c[-1] for c in pkills])
        self.assertTrue(any(c[-3:-2] == ["start"] or "start" in c for c in calls), "start did not follow the stop")


if __name__ == "__main__":
    unittest.main()
