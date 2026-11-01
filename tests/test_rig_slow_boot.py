#!/usr/bin/env python3
"""A slow rig boot is not a dead one (RIG-SLOW-BOOT, 2026-10-28): BOOTX:go's shadow copies are ~1000 UAE-filesystem
round trips, and under host load a boot took ~10 minutes. rig.start's fixed 120 s wait killed such boots as
"frozen" and `ami.py reboot` gave them up at 240 s; the kill + retry looked like a reboot loop from outside.
Both now wait while BOOTX:boot.log keeps changing and give up only on a stall (or rig.BOOT_CAP)."""
import os, pathlib, struct, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, ami


class BootFrozen(unittest.TestCase):
    def test_a_boot_that_still_writes_boot_log_is_not_frozen_however_long_it_takes(self):
        text = "go started\nkit shadows done\nwait done\nenv ok\n"
        self.assertFalse(rig.boot_frozen(text, since_change=10, elapsed=600))

    def test_a_boot_silent_for_the_stall_budget_after_go_started_is_frozen(self):
        self.assertFalse(rig.boot_frozen("go started\n", since_change=rig.BOOT_STALL - 1, elapsed=200))
        self.assertTrue(rig.boot_frozen("go started\n", since_change=rig.BOOT_STALL, elapsed=200))

    def test_before_go_started_only_the_cap_ends_the_wait(self):
        # KS boot, SetPatch and the 060 library's reset write nothing to boot.log
        self.assertFalse(rig.boot_frozen("", since_change=rig.BOOT_STALL * 3, elapsed=rig.BOOT_CAP - 1))
        self.assertTrue(rig.boot_frozen("", since_change=0, elapsed=rig.BOOT_CAP))

    def test_the_last_line_wasabid_started_is_never_frozen(self):
        # wasabid is up: status() answers on the next poll; a stall of the log after it means nothing
        self.assertFalse(rig.boot_frozen("go started\nwasabid started\n", since_change=rig.BOOT_STALL * 2, elapsed=300))


class RebootWaitsForProgress(unittest.TestCase):
    """ami.reboot on a rig: the deadline restarts on every change of boot.log."""

    def run_reboot(self, log_lines_at, wait=240, every=5, up_at=None):
        """Drive ami.reboot with a fake clock. log_lines_at: {second: text} written to boot.log when the clock
        passes that second; up_at: the second from which the marker-list command says the marker is gone."""
        clock = {"now": 0.0}
        d = tempfile.mkdtemp()
        log = pathlib.Path(d) / "boot" / "boot.log"
        log.parent.mkdir()

        def fake_time():
            return clock["now"]

        def fake_sleep(s):
            clock["now"] += s
            for at, text in sorted(log_lines_at.items()):
                if at <= clock["now"]:
                    log.write_text(text)

        def fake_req(code, payload, timeout=None):
            cmd = payload[2:].decode("latin-1")
            if cmd.startswith("Echo"):
                return struct.pack(">I", 0)
            if cmd.startswith("Run"):
                raise OSError("reset cut the answer")
            if cmd.startswith("List"):
                if up_at is not None and clock["now"] >= up_at:
                    return struct.pack(">I", 205)   # object not found: RAM: is new
                raise OSError("down")
            raise AssertionError(cmd)

        with mock.patch.object(ami, "req", fake_req), \
             mock.patch.object(ami.time, "time", fake_time), \
             mock.patch.object(ami.time, "sleep", fake_sleep), \
             mock.patch.object(ami, "boot_log", lambda: log), \
             mock.patch.dict(os.environ, {"UPTERM_RIG": "9"}):
            return ami.reboot(wait=wait, every=every)

    def test_a_slow_boot_whose_log_keeps_changing_is_waited_for_past_the_old_deadline(self):
        lines = {60 * i: "go started\n" + "kit shadow %d\n" % i for i in range(1, 11)}   # a line a minute for 10 min
        took = self.run_reboot(lines, wait=240, up_at=620)
        self.assertGreaterEqual(took, 620)

    def test_a_boot_whose_log_stops_is_given_up_after_the_stall_budget(self):
        with self.assertRaises(SystemExit) as cm:
            self.run_reboot({30: "go started\n"}, wait=240)
        self.assertIn("did not come back within 240 s", str(cm.exception))

    def test_the_a1200_has_no_boot_log_and_keeps_the_plain_deadline(self):
        with mock.patch.dict(os.environ, {"UPTERM_RIG": ""}):
            self.assertIsNone(ami.boot_log())


class StartWaitsForProgress(unittest.TestCase):
    """rig.start: a boot.log that keeps changing for far longer than the old 180 s wait is not killed."""

    def test_start_keeps_waiting_while_boot_log_advances_and_comes_up(self):
        names = ("RIG", "OS", "mine", "START_ARGS", "setup_config_only", "status", "stop", "time", "subprocess",
                 "boot_assigns_missing", "guard_shell_startup", "start_watchdog", "M060")
        saved = {n: getattr(rig, n) for n in names}
        saved_paths = (rig.paths.cap_fsuae_log, rig.paths.FSUAE)
        stops, state = [], {"now": 0.0}

        with tempfile.TemporaryDirectory() as d:
            log = pathlib.Path(d) / "boot" / "boot.log"
            log.parent.mkdir()

            class Clock:
                @staticmethod
                def sleep(s):
                    state["now"] += s
                    # a new boot.log line every 100 s for 10 minutes, wasabid up at 630 s
                    log.write_text("go started\n" + "".join("step %d\n" % i for i in range(int(state["now"] // 100))))

                @staticmethod
                def time():
                    return state["now"]

            class Proc:
                DEVNULL = None

                @staticmethod
                def Popen(*a, **k): pass

            try:
                rig.RIG = pathlib.Path(d)
                rig.OS = "32"
                rig.mine = lambda: False
                rig.START_ARGS = rig.RIG / "start_args.json"
                rig.setup_config_only = lambda: None
                rig.status = lambda quiet=False: state["now"] >= 630
                rig.stop = lambda: stops.append(1)
                rig.time = Clock
                rig.subprocess = Proc
                rig.boot_assigns_missing = lambda: []
                rig.guard_shell_startup = lambda: None
                rig.start_watchdog = lambda: None
                rig.M060 = False
                rig.paths.cap_fsuae_log = lambda **k: None
                rig.paths.FSUAE = pathlib.Path("fs-uae")
                rig.start()   # returns: up, no SystemExit
            finally:
                for n, v in saved.items():
                    setattr(rig, n, v)
                rig.paths.cap_fsuae_log, rig.paths.FSUAE = saved_paths
        self.assertEqual(stops, [], "a slow but advancing boot was killed")
        self.assertGreaterEqual(state["now"], 630)


if __name__ == "__main__":
    unittest.main()
