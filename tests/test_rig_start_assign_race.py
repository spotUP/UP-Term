"""rig.py start waits for BOOTX:go's assigns before it calls the rig up (RIG-START-ASSIGN-RACE, 2026-11-03).

BOOTX:go runs beside the Startup-Sequence (Run >NIL: C:Execute BOOTX:go), and the early S:User-Startup block
(BOOTX:wasabid-up) starts wasabid on its own. wasabid could answer before go reached its assigns: start took that
first answer as "up", read Assign LIST, and failed "the boot lacks the assigns ETC" on a fresh rig 68 whose
boot.log said "assigns done" a moment later."""
import pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig

GO_EARLY = "go started\nearly LIBS: assigns done (before Amarchy)\nkit shadows done\n"
GO_DONE = GO_EARLY + "XCON mounted\nUP-Term shadows done\nassigns done\n"


class GoAssignsDone(unittest.TestCase):
    def test_only_gos_own_line_counts(self):
        self.assertFalse(rig.go_assigns_done(""))
        self.assertFalse(rig.go_assigns_done(GO_EARLY), "the early-LIBS line is not go's assigns")
        self.assertTrue(rig.go_assigns_done(GO_DONE))
        self.assertTrue(rig.go_assigns_done(GO_DONE + "wasabid started\n"))


class StartWaitsForAssigns(unittest.TestCase):
    def test_an_early_wasabid_answer_does_not_end_the_wait_before_go_assigns(self):
        names = ("RIG", "OS", "mine", "START_ARGS", "setup_config_only", "status", "stop", "time", "subprocess",
                 "guard_stale_base", "gc", "refuse_port_clash", "boot_assigns_missing", "guard_shell_startup",
                 "start_watchdog", "retry_wb_reset", "boot_device_check")
        saved = {n: getattr(rig, n) for n in names}
        saved_paths = (rig.paths.cap_fsuae_log, rig.paths.FSUAE)
        checks, stops = [], []

        with tempfile.TemporaryDirectory() as d:
            log = pathlib.Path(d) / "boot/boot.log"

            class Clock:
                now, polls = 0.0, 0

                @classmethod
                def sleep(cls, _):
                    # go is slower than the early wasabid: its assigns land on the fourth poll
                    cls.polls += 1
                    log.parent.mkdir(parents=True, exist_ok=True)
                    log.write_text(GO_DONE if cls.polls >= 4 else GO_EARLY)
                    # start deletes boot.console before the boot; BOOTX:bootcap writes its marker line (the up gate)
                    log.with_name("boot.console").write_text(rig.BOOT_CONSOLE_MARKER + "\n")

                @classmethod
                def time(cls):
                    cls.now += rig.BOOT_POLL
                    return cls.now

            class Proc:
                DEVNULL = None

                @staticmethod
                def Popen(*a, **k): pass

            def assigns_missing():
                checks.append(Clock.polls)
                return [] if "\nassigns done" in log.read_text() else ["ETC"]

            try:
                rig.RIG = pathlib.Path(d)
                rig.OS = "32"
                rig.mine = lambda: False
                rig.guard_stale_base = lambda *a, **k: None
                rig.gc = lambda: []
                rig.refuse_port_clash = lambda port=None: None
                rig.START_ARGS = rig.RIG / "start_args.json"
                rig.setup_config_only = lambda: None
                rig.boot_device_check = lambda: None
                rig.status = lambda quiet=False: True   # the early wasabid answers from the first poll
                rig.stop = lambda: stops.append(1)
                rig.time = Clock
                rig.subprocess = Proc
                rig.boot_assigns_missing = assigns_missing
                rig.guard_shell_startup = lambda: None
                rig.start_watchdog = lambda: None
                rig.retry_wb_reset = lambda: None
                rig.paths.cap_fsuae_log = lambda **k: None
                rig.paths.FSUAE = pathlib.Path("fs-uae")
                with mock.patch.object(rig.rigwasabi, "warn_guru", lambda *a, **k: []):   # would start a wasabi client on a live rig
                    rig.start()   # SystemExit here = the race is back
            finally:
                for n, v in saved.items():
                    setattr(rig, n, v)
                rig.paths.cap_fsuae_log, rig.paths.FSUAE = saved_paths
        self.assertEqual(checks, [4], "Assign LIST is read once, after go wrote 'assigns done'")
        self.assertEqual(stops, [])


if __name__ == "__main__":
    unittest.main()
