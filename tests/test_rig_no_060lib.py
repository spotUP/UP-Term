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

    def test_keep_puts_only_the_named_lib_back(self):
        libs = pathlib.Path(tempfile.mkdtemp())
        src = pathlib.Path(tempfile.mkdtemp())
        old = paths.LIB68060, paths.LIBMMU, paths.LIB680X0
        paths.LIB68060, paths.LIBMMU, paths.LIB680X0 = src / "a", src / "b", src / "c"
        try:
            for f in ("a", "b", "c"):
                (src / f).write_text("x")
            for _l, name, _v in paths.libs060():
                (libs / name).write_text("old")
            rig.drop_060_libs(libs, ("mmu.library",))
            self.assertEqual(sorted(p.name for p in libs.iterdir()), ["mmu.library"])
            self.assertEqual((libs / "mmu.library").read_text(), "x")
        finally:
            paths.LIB68060, paths.LIBMMU, paths.LIB680X0 = old

    def test_keep_flag_is_parsed_from_argv(self):
        old = sys.argv
        sys.argv = old + ["--keep-060lib=68060.library,mmu.library"]
        try:
            self.assertEqual(importlib.reload(rig).KEEP060LIB, ("68060.library", "mmu.library"))
        finally:
            sys.argv = old
            importlib.reload(rig)

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
