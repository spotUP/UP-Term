"""Two `rig.py start` calls begun together launch one FS-UAE, not two.

On 2026-10-30 rig 3 ran two FS-UAE processes on the same build/rig3/sys.hdf
(started 14 s apart) and the Amiga showed a disk checksum error. start()
checked mine() first and launched the emulator seconds later, after the system
disk clone; two starts begun together both saw no emulator in that window and
both launched one. start() now holds <rig dir>/start.lock for the whole boot,
so the second start waits, then finds the first one's emulator.
"""
import pathlib, sys, tempfile, threading, time, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig


class StartRace(unittest.TestCase):
    def test_two_concurrent_starts_launch_one_emulator(self):
        names = ("RIG", "OS", "M060", "mine", "START_ARGS", "setup_config_only", "status", "time", "subprocess",
                 "boot_assigns_missing", "guard_shell_startup", "start_watchdog")
        saved = {n: getattr(rig, n) for n in names}
        saved_paths = (rig.paths.cap_fsuae_log, rig.paths.FSUAE)
        launched = []
        real_sleep = time.sleep

        class Clock:
            @staticmethod
            def sleep(_): real_sleep(0.01)

            @staticmethod
            def time(): return time.time()

        class Proc:
            DEVNULL = None

            @staticmethod
            def Popen(cmd, **k):
                if any("fs-uae" in str(c) for c in cmd):
                    launched.append(cmd)

        with tempfile.TemporaryDirectory() as d:
            try:
                rig.RIG = pathlib.Path(d)
                rig.OS = "32"   # skips the system disk clone
                rig.M060 = False
                rig.mine = lambda: bool(launched)
                rig.START_ARGS = rig.RIG / "start_args.json"
                # the window between the mine() check and the launch (the disk clone on a real rig)
                rig.setup_config_only = lambda: real_sleep(0.3)
                rig.status = lambda quiet=False: bool(launched)
                rig.time = Clock
                rig.subprocess = Proc
                rig.boot_assigns_missing = lambda: []
                rig.guard_shell_startup = lambda: None
                rig.start_watchdog = lambda: None
                rig.paths.cap_fsuae_log = lambda **k: None
                rig.paths.FSUAE = pathlib.Path("fs-uae")
                threads = [threading.Thread(target=rig.start) for _ in range(2)]
                for t in threads:
                    t.start()
                for t in threads:
                    t.join(10)
            finally:
                for n, v in saved.items():
                    setattr(rig, n, v)
                rig.paths.cap_fsuae_log, rig.paths.FSUAE = saved_paths
        self.assertEqual(len(launched), 1, "two starts launched %d emulators on one sys.hdf" % len(launched))


if __name__ == "__main__":
    unittest.main()
