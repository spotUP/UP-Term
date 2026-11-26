"""rig.owner_clash: a start whose --purpose differs from the running rig's owner.json purpose is refused instead of
answering "already running" with rc 0 and then stopping the other chain's rig (RIG16-SHARED, 2026-11-26)."""
import json, pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


def owner(purpose):
    return json.dumps({"root": "/x", "purpose": purpose, "pid": None})


class OwnerClash(unittest.TestCase):
    def test_same_purpose_is_no_clash(self):
        self.assertIsNone(rig.owner_clash(owner("up-term-b3: blink rainbow fix"), "up-term-b3: blink rainbow fix"))

    def test_same_purpose_other_spacing(self):
        self.assertIsNone(rig.owner_clash(owner("up-term-b3:  blink rainbow fix"), "up-term-b3: blink rainbow fix"))

    def test_other_purpose_is_refused(self):
        msg = rig.owner_clash(owner("up-term-b3: blink rainbow fix"), "up-term-b3: 060 crash after up hunt")
        self.assertIn("another purpose", msg)
        self.assertIn("blink rainbow fix", msg)

    def test_no_purpose_given_keeps_old_answer(self):
        self.assertIsNone(rig.owner_clash(owner("up-term-b3: blink rainbow fix"), None))

    def test_no_recorded_purpose_or_file(self):
        self.assertIsNone(rig.owner_clash(owner(None), "x"))
        self.assertIsNone(rig.owner_clash("", "x"))
        self.assertIsNone(rig.owner_clash("not json", "x"))


if __name__ == "__main__":
    unittest.main()
