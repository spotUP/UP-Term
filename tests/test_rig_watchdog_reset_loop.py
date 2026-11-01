#!/usr/bin/env python3
"""The rig watchdog recovers a rig from the Kickstart reset loop (RIG-RESET-LOOP, 2026-10-29).

After a Guru on a 68060 rig, exec's dead-end alert calls the ROM ColdReboot, which keeps the 68060.library's live
CPU state (CACR, TTx, TC, PCR). The next ROM boot dies in the early alert 80000006, flashes and resets again, about
eight times a second, until something restarts the emulator (s18 findings, rig9-installer-loop.md result 8). Nothing
on the Amiga side can answer, so the host watchdog counts the "CPU reset" lines FS-UAE logs and cold-restarts the rig
when the count jumps between two checks. A healthy warm reboot adds one or two lines and must not trigger it."""
import pathlib, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, paths


class ResetCount(unittest.TestCase):
    def test_counts_only_cpu_reset_lines_of_the_live_log(self):
        with tempfile.TemporaryDirectory() as d:
            log = pathlib.Path(d) / "fs-uae.log.txt"
            log.write_text("Illegal instruction: 1050 at 104E8438 -> 00F80AD2\nCPU reset PC=f80dc8\nRTG: mode\n"
                           "CPU reset PC=f800d0\nsome line mentioning CPU reset later\n")
            self.assertEqual(paths.fsuae_reset_count(d), 2)

    def test_no_log_counts_as_zero(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(paths.fsuae_reset_count(d), 0)


class ResetLoopStep(unittest.TestCase):
    def test_the_first_check_only_takes_a_baseline(self):
        calls = []
        self.assertFalse(rig.reset_loop_step(None, 500, lambda: calls.append(1)))
        self.assertEqual(calls, [])

    def test_a_warm_reboot_does_not_trigger_the_recovery(self):
        calls = []
        self.assertFalse(rig.reset_loop_step(10, 12, lambda: calls.append(1)))
        self.assertEqual(calls, [])

    def test_a_jump_of_the_threshold_or_more_restarts_the_rig_once(self):
        calls = []
        self.assertTrue(rig.reset_loop_step(10, 10 + rig.WATCHDOG_RESET_LOOP, lambda: calls.append(1)))
        self.assertEqual(calls, [1])


class RecoverResetLoop(unittest.TestCase):
    def test_recovery_stops_then_starts_with_the_saved_arguments_and_keeps_the_system_disk(self):
        names = ("stop", "subprocess", "reboot_args")
        saved = {n: getattr(rig, n) for n in names}
        events = []

        class Proc:
            DEVNULL = None
            STDOUT = -2

            @staticmethod
            def Popen(argv, **k):
                events.append(("start", argv[2:]))

        with tempfile.TemporaryDirectory() as d:
            log = pathlib.Path(d) / "watchdog.log"
            try:
                rig.stop = lambda: events.append(("stop",))
                rig.subprocess = Proc
                rig.reboot_args = lambda given: ["--max", "--mmu"]
                rig.recover_reset_loop(log)
            finally:
                for n, v in saved.items():
                    setattr(rig, n, v)
            self.assertEqual(events, [("stop",), ("start", ["start", "--keep-sys", "--max", "--mmu"])])
            self.assertIn("reset loop", log.read_text())

    def test_recovery_keeps_the_first_fault_of_the_looping_run_before_the_restart(self):
        """RIG31-RESET-LOOP (2026-10-31): the restart's start truncated the live FS-UAE log, so the guest crash that
        began the loop was never readable afterwards. The recovery saves the log before stop() and names the fault."""
        names = ("stop", "subprocess", "reboot_args")
        saved = {n: getattr(rig, n) for n in names}
        saved_dir = paths.FSUAE_LOG_DIR
        seen_at_stop = []

        class Proc:
            DEVNULL = None
            STDOUT = -2

            @staticmethod
            def Popen(argv, **k):
                pass

        with tempfile.TemporaryDirectory() as d:
            rigdir = pathlib.Path(d)
            logs = rigdir / "logs"
            logs.mkdir()
            (logs / "fs-uae.log.txt").write_text(
                "SetSwitch() - amiga\nIllegal instruction: 000c at 10000A14 -> 00F80AD2\n"
                + "CPU reset PC=f80dc8 (Kickstart ROM)..\nCPU reset PC=f800d0 (Kickstart ROM)..\n" * 12)
            for i in range(4):   # older copies beyond the kept count go
                (rigdir / ("reset-loop-2026010%d-000000.fs-uae.log" % i)).write_text("old")
            log = rigdir / "watchdog.log"
            try:
                paths.FSUAE_LOG_DIR = logs
                rig.stop = lambda: seen_at_stop.append(sorted(p.name for p in rigdir.glob("reset-loop-*")))
                rig.subprocess = Proc
                rig.reboot_args = lambda given: ["--max"]
                rig.recover_reset_loop(log)
            finally:
                paths.FSUAE_LOG_DIR = saved_dir
                for n, v in saved.items():
                    setattr(rig, n, v)
            copies = sorted(rigdir.glob("reset-loop-*.fs-uae.log"))
            self.assertEqual(len(copies), rig.RESET_LOOP_LOGS_KEPT)
            self.assertIn("Illegal instruction: 000c at 10000A14", copies[-1].read_text())
            self.assertEqual(seen_at_stop, [[p.name for p in copies]])   # saved before the emulator stops
            self.assertIn("first fault: Illegal instruction: 000c at 10000A14 -> 00F80AD2", log.read_text())


if __name__ == "__main__":
    unittest.main()
