#!/usr/bin/env python3
"""RIG-STALE-BASE-GUARD: rig.py start refuses when the checkout's HEAD does not contain origin/main, unless
--allow-stale-base carries a non-empty reason (printed). Temp repo with a fake origin."""
import contextlib, io, os, pathlib, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

def git(cwd, *a):
    env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
    return subprocess.run(["git", "-C", str(cwd), *a], check=True, capture_output=True, text=True, env=env).stdout

class StaleBase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        t = pathlib.Path(self.tmp.name)
        self.origin, self.work = t / "origin.git", t / "work"
        git(t, "init", "-q", "--bare", "-b", "main", str(self.origin))
        git(t, "clone", "-q", str(self.origin), str(self.work))
        git(self.work, "checkout", "-q", "-b", "main")
        git(self.work, "commit", "-q", "--allow-empty", "-m", "base")
        git(self.work, "push", "-q", "origin", "main")
        git(self.work, "checkout", "-q", "-b", "feature")

    def tearDown(self):
        self.tmp.cleanup()

    def origin_ahead(self):
        git(self.work, "checkout", "-q", "main")
        git(self.work, "commit", "-q", "--allow-empty", "-m", "landed")
        git(self.work, "push", "-q", "origin", "main")
        git(self.work, "checkout", "-q", "feature")

    def run_guard(self, argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            try:
                rig.guard_stale_base(self.work, argv)
            except SystemExit as e:
                return str(e), out.getvalue()
        return None, out.getvalue()

    def test_head_contains_origin_main_passes(self):
        git(self.work, "commit", "-q", "--allow-empty", "-m", "mine")
        self.assertEqual(self.run_guard(["start"]), (None, ""))

    def test_origin_ahead_is_refused(self):
        self.origin_ahead()
        err, _ = self.run_guard(["start"])
        self.assertIn("RIG-STALE-BASE", err)
        self.assertIn(git(self.work, "rev-parse", "--short", "HEAD").strip(), err)
        self.assertIn(git(self.work, "rev-parse", "--short", "origin/main").strip(), err)
        self.assertIn("1 origin/main commits missing", err)

    def test_override_with_reason_passes_and_prints_it(self):
        self.origin_ahead()
        err, out = self.run_guard(["start", "--allow-stale-base", "bisecting old build"])
        self.assertIsNone(err)
        self.assertIn("bisecting old build", out)

    def test_override_with_empty_reason_is_refused(self):
        self.origin_ahead()
        for argv in (["start", "--allow-stale-base", "  "], ["start", "--allow-stale-base"]):
            err, _ = self.run_guard(argv)
            self.assertIn("RIG-STALE-BASE", err)

if __name__ == "__main__":
    unittest.main()
