"""The kit refuses a tmux binary older than the tmux-amiga sources (TMUX-KIT-REFRESH, 2026-11-02).

Before: dist-base copied tmux-amiga/build/tmux-bin unchecked, so the 17 Oct binary shipped without commit 4beb716.
Now tools/tmuxfresh.py (called by dist-base before the copy) fails when the binary predates the last commit
touching the build inputs, or those inputs have uncommitted changes.
"""
import os, pathlib, shutil, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import tmuxfresh


def git(repo, *a, date=None):
    env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
    if date:
        env["GIT_COMMITTER_DATE"] = env["GIT_AUTHOR_DATE"] = f"{date} +0000"
    subprocess.run(["git", "-C", str(repo), *a], check=True, capture_output=True, env=env)


class Fresh(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.repo = self.d / "tmux-amiga"
        (self.repo / "amiga").mkdir(parents=True)
        (self.repo / "build").mkdir()
        git(self.repo, "init", "-q")
        (self.repo / "amiga/console.c").write_text("v1\n")
        git(self.repo, "add", "amiga/console.c")
        git(self.repo, "commit", "-qm", "v1", date=1000000000)
        self.bin = self.repo / "build/tmux-bin"
        self.bin.write_bytes(b"x")

    def test_binary_newer_than_sources_passes(self):
        self.assertIsNone(tmuxfresh.stale_reason(self.repo, self.bin))

    def test_binary_older_than_last_source_commit_is_refused(self):
        os.utime(self.bin, (1100000000, 1100000000))
        (self.repo / "amiga/console.c").write_text("v2\n")
        git(self.repo, "commit", "-qam", "v2", date=1200000000)
        self.assertIn("older than tmux-amiga commit", tmuxfresh.stale_reason(self.repo, self.bin))

    def test_uncommitted_source_change_is_refused(self):
        (self.repo / "amiga/console.c").write_text("edited\n")
        self.assertIn("uncommitted", tmuxfresh.stale_reason(self.repo, self.bin))

    def test_missing_binary_is_refused(self):
        self.bin.unlink()
        self.assertIn("missing", tmuxfresh.stale_reason(self.repo, self.bin))

    def test_the_real_stale_binary_is_refused_by_the_script(self):
        r = subprocess.run([sys.executable, str(ROOT / "tools/tmuxfresh.py"), str(self.repo), str(self.bin)], capture_output=True)
        self.assertEqual(r.returncode, 0)
        os.utime(self.bin, (1, 1))
        r = subprocess.run([sys.executable, str(ROOT / "tools/tmuxfresh.py"), str(self.repo), str(self.bin)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 1)
        self.assertIn("stale tmux", r.stdout)


if __name__ == "__main__":
    unittest.main()
