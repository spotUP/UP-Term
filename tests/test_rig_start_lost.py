"""rig.boot_lost_after_up: a boot that answered 'up' and then lost the Amiga (guest guru, CPU reset, RIG-RESET-LOOP)
goes to the boot retry instead of failing the start with rc 1 (RIG19-START-FAIL, RIG16-EYE-SETUP-FLAKE)."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class BootLostAfterUp(unittest.TestCase):
    def test_still_up(self):
        self.assertIsNone(rig.boot_lost_after_up(0, 0, True))

    def test_old_resets_do_not_count(self):
        self.assertIsNone(rig.boot_lost_after_up(2, 2, True))

    def test_new_cpu_reset_is_lost_even_when_ping_answers(self):
        self.assertIn("reset", rig.boot_lost_after_up(0, 1, True))

    def test_reset_loop(self):
        self.assertIn("1218", rig.boot_lost_after_up(0, 1218, False))

    def test_no_ping_is_lost(self):
        self.assertIn("wasabid", rig.boot_lost_after_up(0, 0, False))


if __name__ == "__main__":
    unittest.main()
