#!/usr/bin/env python3
"""`rig.py reboot` keeps DH0: (RIG-REBOOT, MASTER-BUILD-NOREBOOT 2026-10-27): it stops the rig and starts it again with
--keep-sys, and that start boots the sys.hdf the last run left instead of a new clone of the master, so an install's
writes survive. Drives rig.main; the emulator, wasabid and xdftool are replaced by sentinels."""
import contextlib, io, json, pathlib, subprocess, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class RigReboot(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        (self.tmp / "sys.hdf").write_bytes(b"installed")
        self.calls = []
        st = contextlib.ExitStack()
        self.addCleanup(st.close)
        p = lambda name, new: st.enter_context(mock.patch.object(rig, name, new))
        p("RIG", self.tmp)
        p("START_ARGS", self.tmp / "start.args")
        p("stop", lambda: self.calls.append("stop"))
        p("fresh_sys_hdf", lambda: self.calls.append("clone"))
        for name in ("sys_hdf_screenmode", "sys_hdf_monitors", "sys_hdf_no_wasabid", "sys_hdf_shadows_block", "sys_hdf_no_dopus",
                     "sys_hdf_dopus", "sys_hdf_kit", "sys_hdf_cpu_lib"):
            p(name, lambda image, _n=name: self.calls.append(_n))
        p("mine", lambda: [])
        p("gc", lambda: [])   # the start sweep reads the real build/ folders
        p("refuse_port_clash", lambda port=None: None)   # lsof sees the real machine
        p("setup_config_only", lambda: None)
        p("status", lambda quiet=False: True)
        p("go_assigns_done", lambda text: True)   # the boot.log of a real go (RIG-START-ASSIGN-RACE)
        p("boot_assigns_missing", lambda: [])
        p("guard_shell_startup", lambda: None)
        p("retry_wb_reset", lambda: None)
        st.enter_context(mock.patch.object(rig.rigwasabi, "warn_guru", lambda *a, **k: []))   # would start a wasabi client on a live rig
        st.enter_context(mock.patch.object(rig.paths, "cap_fsuae_log", lambda **k: None))
        st.enter_context(mock.patch.object(rig.subprocess, "Popen", lambda *a, **k: None))
        def sleep(s):   # start deletes boot/boot.console before the boot; BOOTX:bootcap writes its marker line (the up gate)
            (self.tmp / "boot").mkdir(exist_ok=True)
            (self.tmp / "boot/boot.console").write_text(rig.BOOT_CONSOLE_MARKER + "\n")
        st.enter_context(mock.patch.object(rig.time, "sleep", sleep))
        st.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def boot(self, keep, argv):
        with mock.patch.object(rig, "KEEP_SYS", keep), mock.patch.object(rig.sys, "argv", argv), \
             mock.patch.object(rig.subprocess, "run", lambda *a, **k: subprocess.CompletedProcess(a, 0)):
            rig.start()

    def test_reboot_stops_then_starts_with_keep_sys_and_the_last_flags(self):
        (self.tmp / "start.args").write_text(json.dumps(["--max", "--purpose", "s: t"]))
        ran = []
        with mock.patch.object(rig.subprocess, "run", lambda cmd, **k: ran.append(cmd)), \
             mock.patch.object(rig.sys, "argv", ["rig.py", "reboot"]):
            self.assertEqual(rig.main(["rig.py", "reboot"]), 0)
        self.assertEqual(self.calls, ["stop"])
        self.assertEqual(ran[0][2:], ["start", "--keep-sys", "--max", "--purpose", "s: t"])

    def test_keep_sys_start_boots_the_disk_the_last_run_left(self):
        self.boot(True, ["rig.py", "start", "--keep-sys", "--max"])
        self.assertNotIn("clone", self.calls)
        self.assertEqual(self.calls, ["sys_hdf_no_wasabid", "sys_hdf_shadows_block"])   # wasabid and shadow blocks go again; nothing else
        self.assertEqual((self.tmp / "sys.hdf").read_bytes(), b"installed")
        self.assertEqual(json.loads((self.tmp / "start.args").read_text()), ["--max"])   # a later reboot or aga clones again

    def test_plain_start_still_clones(self):
        self.boot(False, ["rig.py", "start", "--max"])
        self.assertEqual(self.calls[0], "clone")

    def test_keep_sys_without_a_disk_clones(self):
        (self.tmp / "sys.hdf").unlink()
        self.boot(True, ["rig.py", "start", "--keep-sys"])
        self.assertEqual(self.calls[0], "clone")


if __name__ == "__main__":
    unittest.main()
