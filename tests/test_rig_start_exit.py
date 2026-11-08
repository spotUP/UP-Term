"""rig.py start fails (non-zero exit) when every boot attempt fails.

On 2026-10-25 getty_rig ran `rig.py start --serial` on rig 1 with check=True.
All three boots printed '[ERROR] no wasabid answer', yet start returned rc 0:
after the last retry start() fell off its end. getty_rig went on against a rig
that never came up and died in wasabi Conn with ConnectionRefusedError. The
third boot's FS-UAE was never stopped and ran on with its logwatch.
"""
import contextlib, io, pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig


import emuwin


class StartExit(unittest.TestCase):
    def failed_start(self, events, screen):
        """rig.start() with a boot that never answers; events records each stop and each screen capture in order."""
        names = ("RIG", "OS", "mine", "START_ARGS", "setup_config_only", "status", "stop", "time", "subprocess", "guard_stale_base", "gc", "refuse_port_clash")
        saved = {n: getattr(rig, n) for n in names}
        saved_paths = (rig.paths.cap_fsuae_log, rig.paths.FSUAE, rig.paths.reports_dir)
        saved_window_text = emuwin.window_text

        class Clock:
            # a slow boot is waited for (boot_frozen): the clock must advance, or the wait never ends
            now = 0.0

            @staticmethod
            def sleep(_): pass

            @classmethod
            def time(cls):
                cls.now += rig.BOOT_POLL
                return cls.now

        class Proc:
            DEVNULL = None

            @staticmethod
            def Popen(*a, **k): pass

        with tempfile.TemporaryDirectory() as d:
            try:
                rig.RIG = pathlib.Path(d)
                rig.OS = "32"   # skips the system disk clone
                rig.mine = lambda: False
                rig.guard_stale_base = lambda *a, **k: None   # git checks against the real checkout
                rig.gc = lambda: []   # the start sweep reads the real build/ folders
                rig.refuse_port_clash = lambda port=None: None   # lsof sees the real machine
                rig.START_ARGS = rig.RIG / "start_args.json"
                rig.setup_config_only = lambda: None
                rig.status = lambda quiet=False: False   # wasabid never answers
                rig.stop = lambda: events.append("stop")
                rig.time = Clock
                rig.subprocess = Proc
                rig.paths.cap_fsuae_log = lambda **k: None
                rig.paths.FSUAE = pathlib.Path("fs-uae")
                rig.paths.reports_dir = lambda *p: pathlib.Path(d)

                def window_text(cfg, out_dir, name, top=1.0):
                    events.append("screen")
                    return pathlib.Path(d) / "boot-freeze.png", screen

                emuwin.window_text = window_text
                with self.assertRaises(SystemExit) as cm:
                    rig.start()
            finally:
                for n, v in saved.items():
                    setattr(rig, n, v)
                rig.paths.cap_fsuae_log, rig.paths.FSUAE, rig.paths.reports_dir = saved_paths
                emuwin.window_text = saved_window_text
        return cm

    def test_start_exits_non_zero_after_the_last_failed_boot(self):
        events = []
        cm = self.failed_start(events, [])
        stops = [e for e in events if e == "stop"]
        self.assertNotIn(cm.exception.code, (None, 0), "start must fail the command after its last retry")
        self.assertEqual(len(stops), rig.START_RETRIES + 1,
                         "every failed boot is stopped, the last one too: its FS-UAE must not keep running")


    def test_each_frozen_boot_screen_is_read_before_its_stop(self):
        # rig 94, 2026-11-08: the owner saw "Please insert volume System: in any drive"; the retry stopped the
        # emulator and nothing recorded the requester. Each failed boot is now captured before it is stopped.
        events = []
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.failed_start(events, ["Workbench Screen", "Please insert volume System: in any drive", "Retry Cancel"])
        self.assertEqual(events, ["screen", "stop"] * (rig.START_RETRIES + 1))
        self.assertIn("[ERROR] frozen boot screen: Please insert volume System: in any drive", out.getvalue())
        self.assertNotIn("frozen boot screen: Workbench Screen", out.getvalue())


class RequesterLines(unittest.TestCase):
    def test_keeps_requester_and_alert_lines_only(self):
        lines = ["Workbench Screen", "PLEASE INSERT volume System: in any drive", "Retry Cancel",
                 "Software Failure. Press left mouse button to continue.", "Guru Meditation #80000004.0000AAAA"]
        self.assertEqual(emuwin.requester_lines(lines), lines[1:2] + lines[3:])


if __name__ == "__main__":
    unittest.main()
