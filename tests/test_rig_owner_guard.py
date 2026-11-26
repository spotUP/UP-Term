"""rig.foreign_owner / refuse_foreign_owner: `start` on a running rig and `stop` refuse a rig that another live owner
(other --purpose, owner session pid still running) holds (RIG16-SHARED: two chains on rig 16 stopped each other)."""
import json, os, pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

alive = lambda pid: False
dead = lambda pid: True


class ForeignOwner(unittest.TestCase):
    def test_other_live_owner_is_foreign(self):
        self.assertEqual(rig.foreign_owner({"purpose": "b: blink", "pid": 5}, "b: 060 hunt", alive), "b: blink")

    def test_same_purpose_is_own(self):
        self.assertIsNone(rig.foreign_owner({"purpose": "b: blink", "pid": 5}, "b: blink", alive))

    def test_dead_owner_is_free(self):
        self.assertIsNone(rig.foreign_owner({"purpose": "b: blink", "pid": 5}, "b: 060 hunt", dead))

    def test_no_record_or_no_purpose_is_free(self):
        self.assertIsNone(rig.foreign_owner(None, "x", alive))
        self.assertIsNone(rig.foreign_owner({"purpose": None, "pid": 5}, "x", alive))

    def test_caller_without_purpose_is_refused(self):
        self.assertEqual(rig.foreign_owner({"purpose": "b: blink", "pid": 5}, None, alive), "b: blink")

    def test_real_owner_gone_on_dead_pid(self):
        self.assertIsNone(rig.foreign_owner({"purpose": "b: blink", "pid": 2 ** 22 + 12345}, "other"))


class Refuse(unittest.TestCase):
    def run_refuse(self, rec, purpose):
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / "owner.json"
            if rec is not None:
                f.write_text(json.dumps(rec))
            with mock.patch.object(rig, "OWNER_FILE", f), mock.patch.object(rig, "PURPOSE", purpose):
                rig.refuse_foreign_owner()

    def test_refuses_with_owner_text(self):
        with self.assertRaises(SystemExit) as e:
            self.run_refuse({"purpose": "b: blink", "pid": os.getpid()}, "b: 060 hunt")
        self.assertRegex(str(e.exception.code), r"^\[ERROR\] rig \d+ owned by b: blink$")

    def test_same_purpose_passes(self):
        self.run_refuse({"purpose": "b: blink", "pid": os.getpid()}, "b: blink")

    def test_no_file_passes(self):
        self.run_refuse(None, "x")


if __name__ == "__main__":
    unittest.main()
