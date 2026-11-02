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

    def test_one_apply_or_restore_at_a_time(self):
        """ICONS-APPLY-DOUBLE (rig 61): two APPLY copies raced on the manifest. The lock is taken before APPLY/RESTORE reads the
        manifest and released at out:, so a second copy waits instead of racing."""
        s = (ROOT / "amarchy/icons.c").read_text()
        main = s[s.index("int main(void)"):]
        take = main.index("lock = ia_lock_take();")
        self.assertLess(take, main.index("if (cmd == IA_CMD_APPLY) {"))
        self.assertLess(take, main.index("ia_restore(c);"))
        self.assertIn("if (lock) ReleaseSemaphore(lock);", main[main.index("\nout:"):])
        helper = s[s.index("static struct SignalSemaphore *ia_lock_take"):s.index("int main(void)")]
        self.assertRegex(helper, r"Forbid\(\);\s*l = \(struct ia_lock \*\)FindSemaphore")
        self.assertIn("ObtainSemaphore(&l->ss);", helper)

    def test_links_are_not_walked_as_drawers(self):
        """RL11 (rig 73): RAM Disk:Disk.info is a soft link (fib_DirEntryType ST_SOFTLINK = 3 > 0); list_dir walked it as a drawer
        and printed 'skip io (no lock): RAM Disk:Disk.info/'. Links are skipped before the drawer test."""
        s = (ROOT / "amarchy/icons.c").read_text()
        body = s[s.index("static int list_dir"):]
        body = body[:body.index("\n}\n")]
        skip = re.search(r"if \(fib->fib_DirEntryType == ST_SOFTLINK \|\| fib->fib_DirEntryType == ST_LINKDIR\) continue;", body)
        self.assertIsNotNone(skip)
        self.assertLess(skip.start(), body.index("if (fib->fib_DirEntryType > 0) kind = 'D';"))


if __name__ == "__main__":
    unittest.main()
