"""paths.rig_flags splits UPTERM_RIG_FLAGS the shell way: a quoted --purpose stays one word (it was split on
whitespace, so rig window titles came out wrong)."""
import os, pathlib, sys, unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import paths


class RigFlags(unittest.TestCase):
    def test_quoted_purpose_stays_one_word(self):
        with mock.patch.dict(os.environ, {"UPTERM_RIG_FLAGS": '--max --purpose "Amarchy: menu check"'}):
            self.assertEqual(paths.rig_flags(), ["--max", "--purpose", "Amarchy: menu check"])

    def test_unset_gives_default_copy(self):
        with mock.patch.dict(os.environ, clear=False):
            os.environ.pop("UPTERM_RIG_FLAGS", None)
            d = ["--stock"]
            self.assertEqual(paths.rig_flags(d), ["--stock"])
            self.assertIsNot(paths.rig_flags(d), d)
            self.assertEqual(paths.rig_flags(), [])


if __name__ == "__main__":
    unittest.main()
