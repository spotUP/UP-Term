#!/usr/bin/env python3
"""rig.boot_device_check refuses to launch the emulator without a boot device: a missing config, no first hard drive
in it, or a first hard drive that is missing or empty exits; a present non-empty one passes. Both config dialects:
FS-UAE `hard_drive_0 = <path>` and Amiberry `uaehf0=dir|hdf,...` (amiberry.drive_line). _start must call it before
any launch: with an empty boot device no emulator process is started."""
import pathlib, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, paths, amiberry


class BootDeviceCheck(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(__import__("shutil").rmtree, self.d)
        self.cfg = self.d / "cfg"
        for patch in (mock.patch.object(rig, "CFG", self.cfg), mock.patch.object(rig, "AMIBERRY", False)):
            patch.start()
            self.addCleanup(patch.stop)

    def write(self, text):
        self.cfg.write_text(text)

    def test_missing_config_exits(self):
        with self.assertRaises(SystemExit) as e:
            rig.boot_device_check()
        self.assertIn("missing", str(e.exception))

    def test_no_hard_drive_exits(self):
        self.write("hard_drive_1 = /x\n")
        with self.assertRaises(SystemExit) as e:
            rig.boot_device_check()
        self.assertIn("no boot device", str(e.exception))

    def test_missing_and_empty_target_exit(self):
        empty_dir, empty_file = self.d / "emptydir", self.d / "empty.hdf"
        empty_dir.mkdir()
        empty_file.write_bytes(b"")
        for target in (self.d / "nope", empty_dir, empty_file):
            self.write("hard_drive_0 = %s\n" % target)
            with self.assertRaises(SystemExit, msg=str(target)):
                rig.boot_device_check()

    def test_present_non_empty_target_passes(self):
        full_dir, full_file = self.d / "dir", self.d / "sys.hdf"
        full_dir.mkdir()
        (full_dir / "S").write_text("x")
        full_file.write_bytes(b"x")
        for target in (full_dir, full_file):
            self.write("hard_drive_0 = %s\n" % target)
            rig.boot_device_check()

    def test_amiberry_dir_and_hdf_lines(self):
        full = self.d / "os"
        full.mkdir()
        (full / "S").write_text("x")
        hdf = self.d / "sys.hdf"
        hdf.write_bytes(b"x")
        with mock.patch.object(rig, "AMIBERRY", True):
            for d in (dict(path=str(full), hdf=False, label="Empty", pri=10), dict(path=str(hdf), hdf=True, pri=0)):
                self.write(amiberry.drive_line(0, d) + "\n")
                rig.boot_device_check()
            self.write(amiberry.drive_line(0, dict(path=str(self.d / "gone"), hdf=False, label="E", pri=0)) + "\n")
            with self.assertRaises(SystemExit):
                rig.boot_device_check()


class StartCallsGuard(unittest.TestCase):
    def test_start_refuses_before_launch_with_empty_boot_device(self):
        d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(__import__("shutil").rmtree, d)
        cfg = d / "cfg"
        (d / "empty").mkdir()
        cfg.write_text("hard_drive_0 = %s\n" % (d / "empty"))
        launched = AssertionError("emulator launched without a boot device")   # fails fast if the guard is missing
        popen, run = mock.MagicMock(side_effect=launched), mock.MagicMock(side_effect=launched)
        with mock.patch.multiple(rig, CFG=cfg, RIG=d / "rig", START_ARGS=d / "args", AMIBERRY=False, OS="x", OS32=False,
                                 mine=lambda: [], refuse_port_clash=lambda: None, setup_config_only=lambda: None), \
             mock.patch.object(paths, "EMU_FILE", d / "emu"), \
             mock.patch.object(paths, "cap_fsuae_log", lambda **k: None), \
             mock.patch.object(rig, "boot_device_check", wraps=rig.boot_device_check) as guard, \
             mock.patch.object(rig.subprocess, "Popen", popen), mock.patch.object(rig.subprocess, "run", run):
            with self.assertRaises(SystemExit):
                rig._start(0)
        guard.assert_called_once()
        popen.assert_not_called()
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
