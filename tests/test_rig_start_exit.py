"""rig.py start fails (non-zero exit) when every boot attempt fails.

On 2026-10-25 getty_rig ran `rig.py start --serial` on rig 1 with check=True.
All three boots printed '[ERROR] no wasabid answer', yet start returned rc 0:
after the last retry start() fell off its end. getty_rig went on against a rig
that never came up and died in wasabi Conn with ConnectionRefusedError. The
third boot's FS-UAE was never stopped and ran on with its logwatch.
"""
import pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig


class StartExit(unittest.TestCase):
    def test_start_exits_non_zero_after_the_last_failed_boot(self):
        names = ("RIG", "OS", "mine", "START_ARGS", "setup_config_only", "status", "stop", "time", "subprocess")
        saved = {n: getattr(rig, n) for n in names}
        saved_paths = (rig.paths.cap_fsuae_log, rig.paths.FSUAE)
        stops = []

        class Clock:
            @staticmethod
            def sleep(_): pass

        class Proc:
            DEVNULL = None

            @staticmethod
            def Popen(*a, **k): pass

        with tempfile.TemporaryDirectory() as d:
            try:
                rig.RIG = pathlib.Path(d)
                rig.OS = "32"   # skips the system disk clone
                rig.mine = lambda: False
                rig.START_ARGS = rig.RIG / "start_args.json"
                rig.setup_config_only = lambda: None
                rig.status = lambda quiet=False: False   # wasabid never answers
                rig.stop = lambda: stops.append(1)
                rig.time = Clock
                rig.subprocess = Proc
                rig.paths.cap_fsuae_log = lambda **k: None
                rig.paths.FSUAE = pathlib.Path("fs-uae")
                with self.assertRaises(SystemExit) as cm:
                    rig.start()
            finally:
                for n, v in saved.items():
                    setattr(rig, n, v)
                rig.paths.cap_fsuae_log, rig.paths.FSUAE = saved_paths
        self.assertNotIn(cm.exception.code, (None, 0), "start must fail the command after its last retry")
        self.assertEqual(len(stops), rig.START_RETRIES + 1,
                         "every failed boot is stopped, the last one too: its FS-UAE must not keep running")


if __name__ == "__main__":
    unittest.main()
