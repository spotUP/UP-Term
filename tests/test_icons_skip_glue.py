"""ICONS-SKIP-NAMES: every skip in the amarchy-icons glue (amarchy/icons.c) goes through ia_skip_note, which counts it
and names it ("skip <reason>: <path>"). A bare counter bump there leaves a skip the APPLY output counts but never names:
rig 16 printed skipped-io=14 with only 13 io lines, the 14th being a drawer walk_dir could not lock.
Source pin of the glue; the core is tested in tests/test_iconapply.c test_skip_names."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


class IconsSkipGlue(unittest.TestCase):
    def test_no_bare_skip_counter_in_glue(self):
        s = (ROOT / "amarchy/icons.c").read_text()
        self.assertNotRegex(s, r"->skipped_[a-z]+\s*\+\+")

    def test_walk_dir_lock_failure_is_named(self):
        s = (ROOT / "amarchy/icons.c").read_text()
        body = s[s.index("static void walk_dir"):]
        body = body[:body.index("\n}\n")]
        self.assertRegex(body, r"if \(!lock\) \{ ia_skip_note\(c, &c->skipped_io, g_wpath,")


if __name__ == "__main__":
    unittest.main()
