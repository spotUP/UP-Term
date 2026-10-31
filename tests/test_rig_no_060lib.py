"""RIG31-RESET-LOOP s40: --no-060lib boots the 060 machine with none of the 060 CPU libraries in Libs:."""
import importlib, pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig
import paths


class No060Lib(unittest.TestCase):
    def test_drop_removes_every_060_lib_and_keeps_the_stub(self):
        libs = pathlib.Path(tempfile.mkdtemp())
        for _l, name, _v in paths.libs060():
            (libs / name).write_text("x")
        (libs / "68040.library").write_text("stub")
        rig.drop_060_libs(libs)
        self.assertEqual([p.name for p in libs.iterdir()], ["68040.library"])

    def test_flag_is_parsed_from_argv(self):
        self.assertFalse(rig.NO060LIB)
        old = sys.argv
        sys.argv = old + ["--no-060lib"]
        try:
            self.assertTrue(importlib.reload(rig).NO060LIB)
        finally:
            sys.argv = old
            importlib.reload(rig)


if __name__ == "__main__":
    unittest.main()
