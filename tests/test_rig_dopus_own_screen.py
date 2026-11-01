"""D7 own screen (s39): `rig.py dopus --own-screen` writes the saved DOpus environment (DOpus5/Environment/workbench) that opens
DOpus 5.104 on its own 1344x1080 screen. Host test on a shipped-style file: the 24 bytes change, nothing else, twice is the
same, and a foreign file is refused."""
import pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import dopus_rig as d


class OwnScreen(unittest.TestCase):
    def setUp(self):
        self.vtc = pathlib.Path(tempfile.mkdtemp())
        (self.vtc / "DOpus5/Environment").mkdir(parents=True)
        self.f = self.vtc / d.ENVFILE
        shipped = bytearray(5428)
        for off, (old, _) in d.OWN_SCREEN.items():
            shipped[off] = old
        shipped[100] = 77   # a byte the step must not touch
        self.f.write_bytes(bytes(shipped))
        self.shipped = bytes(shipped)

    def test_writes_the_mode_and_only_it(self):
        d.own_screen(self.vtc)
        got = self.f.read_bytes()
        self.assertEqual(got[28:32], bytes([5, 64, 4, 56]))   # 1344 x 1080
        self.assertEqual([i for i in range(len(got)) if got[i] != self.shipped[i]], sorted(d.OWN_SCREEN))
        d.own_screen(self.vtc)
        self.assertEqual(self.f.read_bytes(), got)

    def test_refuses_a_foreign_file(self):
        b = bytearray(self.shipped)
        b[28] = 9
        self.f.write_bytes(bytes(b))
        with self.assertRaises(SystemExit):
            d.own_screen(self.vtc)

    def test_flag_runs_it_when_installed(self):
        with mock.patch.object(d.rig, "RIG", self.vtc.parent), mock.patch.object(d.rig, "dopus_archive_md5", lambda: "abc"), \
                mock.patch.object(d, "own_screen") as step, mock.patch.object(sys, "argv", ["dopus", "--own-screen"]):
            (self.vtc.parent / "vtc").mkdir(exist_ok=True)
            vtc = self.vtc.parent / "vtc"
            (vtc / d.rig.DOPUS_STAMP).write_text("abc\n")
            d.main()
        step.assert_called_once()


if __name__ == "__main__":
    unittest.main()
