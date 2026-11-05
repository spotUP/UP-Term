"""RIG-GURU-LOST-ON-RESET: a Guru resets the guest and T:lastguru goes with it, so the rig keeps a reconnecting,
appending debug stream on the host and rig.py prints "[WARN] Guru before reset: <line>" after a (re)boot (host only)."""
import contextlib, io, pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import wasabi as wb

OPEN = "2026-11-05 10:00:00.000 client | debug stream open to 127.0.0.1:1383 (wasabid 0.11b2), --x"
GURU = ("2026-11-05 10:00:00.001 debug | [wasabi: last guru: DEADEND ALERT #80000008 (CPU: privilege violation) in "
        "task 'RAM:bbprobe' (0x1013d268) - from before the last reboot (seen 05-Nov-26 09:56:35)]")
CLOSED = ("2026-11-05 10:00:09.000 client | debug: connection lost (connection closed by the Amiga) - reconnecting "
          "until the Amiga returns; Ctrl-C to stop")
BACK = "2026-11-05 10:00:33.000 client | debug: reconnected after 24 s - wasabid 0.11b2"


class Parser(unittest.TestCase):
    def test_finds_only_new_alert_lines(self):
        t1 = OPEN + "\n" + GURU + "\n"
        lines, off = wb.new_alerts(t1, 0)
        self.assertEqual(len(lines), 1)
        self.assertIn("#80000008", lines[0])
        t2 = t1 + CLOSED + "\n" + OPEN + "\n"
        self.assertEqual(wb.new_alerts(t2, off)[0], [])
        t3 = t2 + GURU.replace("09:56:35", "10:05:00") + "\n"
        self.assertEqual(len(wb.new_alerts(t3, off)[0]), 1)

    def test_client_lines_never_match(self):
        self.assertEqual(wb.new_alerts("x client | ALERT #80000008 note\n", 0)[0], [])

    def test_reconnect_state(self):
        self.assertFalse(wb.stream_reconnected(OPEN + "\n" + CLOSED + "\n"))
        self.assertTrue(wb.stream_reconnected(CLOSED + "\n" + BACK + "\n"))


class Capture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = pathlib.Path(self.tmp.name)
        for name, val in (("out_dir", lambda rig=None: self.out),):
            p = mock.patch.object(wb, name, val)
            p.start()
            self.addCleanup(p.stop)

    def test_debug_stream_reconnects_and_appends(self):
        (self.out / "debug.log").write_text("OLD BOOT\n")
        with mock.patch.object(wb.subprocess, "Popen") as popen:
            popen.return_value.pid = 4242
            wb.start_stream("debug")
        cmd = popen.call_args[0][0]
        self.assertNotIn("--once", cmd)
        self.assertEqual((self.out / "debug.log").read_text(), "OLD BOOT\n")
        with mock.patch.object(wb.subprocess, "Popen") as popen:
            popen.return_value.pid = 4243
            wb.start_stream("snoop")
        self.assertIn("--once", popen.call_args[0][0])

    def test_warn_prints_once_after_the_reconnect(self):
        log = self.out / "debug.log"
        log.write_text(OPEN + "\n" + CLOSED + "\n")
        ticks = []

        def sleep(_):
            ticks.append(1)
            log.write_text(OPEN + "\n" + CLOSED + "\n" + BACK + "\n" + GURU + "\n")

        buf = io.StringIO()
        with mock.patch.object(wb, "stream_alive", return_value=True), contextlib.redirect_stdout(buf):
            first = wb.warn_guru(timeout=5, sleep=sleep)
            again = wb.warn_guru(timeout=5, sleep=sleep)
        self.assertEqual(len(first), 1)
        self.assertEqual(again, [])
        self.assertEqual(ticks, [1])
        self.assertEqual(buf.getvalue().count("[WARN] Guru before reset: [wasabi: last guru: DEADEND ALERT #80000008"), 1)


if __name__ == "__main__":
    unittest.main()
