#!/usr/bin/env python3
"""`rig.py hold` (RIG-EYECHECK-HOLD): a rig left up for the owner's eye check is kept off the idle watchdog's list by a
process that holds paths.long_run(). The hold makes long_run_active true; time up, the rig stopping, a newer hold
removing hold.pid and `hold --release` (SIGTERM) all clear it; the watchdog's own check (watchdog_step) leaves the rig
alone while it lasts. Injected clock and sleep, one short real subprocess for the signal path, no emulator."""
import pathlib, subprocess, sys, tempfile, time, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig, paths

IDLE_FOR_HOURS = 3600.0 * 5   # the last command is this old: the watchdog would stop an unheld rig (idle 15 min)


class Clock:
    def __init__(self):
        self.t = 1000.0
    def __call__(self):
        return self.t


class HoldLoop(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.d = pathlib.Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)
        self.clock = Clock()
        self.stopped = []

    def watchdog(self):
        """The watchdog's idle check against this rig folder; True when it would stop the rig."""
        return rig.watchdog_step(self.clock() + IDLE_FOR_HOURS, 7, 15, self.clock(), paths.long_run_active(self.d),
                                 lambda: self.stopped.append(1))

    def run_hold(self, hours, on_sleep, running=lambda: True):
        seen = []
        def sleep(s):
            self.clock.t += s
            seen.append((paths.long_run_active(self.d), (self.d / "hold.pid").exists(), self.watchdog()))
            on_sleep(len(seen))
        rig.hold_run(hours, self.d, now=self.clock, sleep=sleep, running=running, poll=60)
        return seen

    def test_hold_sets_long_run_and_watchdog_leaves_the_rig(self):
        seen = self.run_hold(1, lambda n: None)
        self.assertEqual(len(seen), 60)   # 1 h in 60 s polls: ran to its deadline
        self.assertTrue(all(active and pid and not stop for active, pid, stop in seen))
        self.assertEqual(self.stopped, [])
        self.assertFalse(paths.long_run_active(self.d))   # expiry cleared it
        self.assertFalse((self.d / "hold.pid").exists())
        self.assertIsNone(rig.hold_info(self.d))
        self.assertTrue(self.watchdog())   # no hold, idle: the watchdog stops the rig again
        self.assertEqual(self.stopped, [1])

    def test_rig_stopping_ends_the_hold(self):
        alive = [True]
        seen = self.run_hold(4, lambda n: alive.__setitem__(0, n < 3), running=lambda: alive[0])
        self.assertEqual(len(seen), 3)
        self.assertFalse(paths.long_run_active(self.d))
        self.assertFalse((self.d / "hold.pid").exists())

    def test_hold_pid_removed_by_a_newer_hold_or_release_ends_it(self):
        seen = self.run_hold(4, lambda n: (self.d / "hold.pid").unlink() if n == 2 else None)
        self.assertEqual(len(seen), 2)
        self.assertFalse(paths.long_run_active(self.d))

    def test_hold_info_reports_the_live_hold(self):
        def look(n):
            info = rig.hold_info(self.d)
            self.assertIsNotNone(info)
            self.assertEqual(info[1], 1000.0 + 3600)
        self.run_hold(1, look)


class HoldProcess(unittest.TestCase):
    def test_release_signals_the_holder_and_clears_its_flag(self):
        with tempfile.TemporaryDirectory() as t:
            d = pathlib.Path(t)
            code = ("import sys; sys.path.insert(0, %r); import rig; "
                    "rig.hold_run(1, %r, running=lambda: True, poll=0.05)" % (str(ROOT / "tools/rig"), str(d)))
            p = subprocess.Popen([sys.executable, "-c", code])
            try:
                for _ in range(100):
                    if rig.hold_info(d) and paths.long_run_active(d):
                        break
                    time.sleep(0.02)
                self.assertEqual(rig.hold_info(d)[0], p.pid)
                self.assertTrue(paths.long_run_active(d))
                self.assertTrue(rig.hold_release(d))
                p.wait(timeout=1)
                self.assertFalse(paths.long_run_active(d))
                self.assertFalse((d / "hold.pid").exists())
                self.assertFalse(rig.hold_release(d))   # nothing left to release
            finally:
                if p.poll() is None:
                    p.kill()
                    p.wait()


if __name__ == "__main__":
    unittest.main()
