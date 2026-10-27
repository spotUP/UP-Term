"""rig.py --debugger: console_debugger = 1 in the FS-UAE config, a FIFO for stdin, a log for stdout; `rig.py dbg`
writes one line into the FIFO."""
import os, pathlib, sys, tempfile, unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import rig


class RigDebugger(unittest.TestCase):
    def test_config_line_only_with_flag(self):
        with mock.patch.object(rig, "DEBUGGER", True):
            self.assertEqual(rig.debugger_lines(), ["console_debugger = 1"])
        with mock.patch.object(rig, "DEBUGGER", False):
            self.assertEqual(rig.debugger_lines(), [])
            self.assertEqual(rig.fsuae_stdio()[0], rig.subprocess.DEVNULL)

    def test_dbg_line_reaches_fifo(self):
        with tempfile.TemporaryDirectory() as d:
            fifo = pathlib.Path(d) / "debugger.in"
            log = pathlib.Path(d) / "debugger.log"
            with mock.patch.object(rig, "DEBUGGER", True), mock.patch.object(rig, "DBG_IN", fifo), \
                    mock.patch.object(rig, "DBG_LOG", log):
                fin, fout, _ = rig.fsuae_stdio()
                with mock.patch.object(sys, "argv", ["rig.py", "dbg", "w", "0", "1FB88", "4", "W"]):
                    rig.dbg()
                self.assertEqual(os.read(fin, 100), b"w 0 1FB88 4 W\n")
                os.close(fin)
                fout.close()
                self.assertTrue(log.exists())


if __name__ == "__main__":
    unittest.main()
