"""The rig tools take the upterm-ports checkout from paths.ports(), which honours UPTERM_PORTS.

upkg_browse_rig, upkg_ask_rig and shstdin_rig built the path from paths.repo("upterm-ports") and ignored
UPTERM_PORTS, so a run against the kit worktree (.claude/worktrees/kit-main) looked for upkg in the shared
checkout and stopped (ledger row 2120, 2026-10-29).
"""
import os, pathlib, re, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import paths

# A path to the ports checkout built anywhere but paths.ports(): repo("upterm-ports") or a
# '<dir> / "upterm-ports' join. Prose mentions in docstrings and messages do not match.
BUILT = re.compile(r"""repo\(\s*['"]upterm-ports['"]|/\s*['"]upterm-ports""")


class Ports(unittest.TestCase):
    def test_env_moves_the_checkout(self):
        with mock.patch.dict(os.environ, {"UPTERM_PORTS": "/x/kit-main"}):
            self.assertEqual(paths.ports(), pathlib.Path("/x/kit-main"))

    def test_default_is_the_workspace_checkout(self):
        env = {k: v for k, v in os.environ.items() if k != "UPTERM_PORTS"}
        with mock.patch.dict(os.environ, env, clear=True):
            self.assertEqual(paths.ports(), paths.repo("upterm-ports"))

    def test_no_tool_builds_the_path_itself(self):
        hits = []
        for f in sorted((ROOT / "tools").rglob("*.py")):
            if f == ROOT / "tools/rig/paths.py":
                continue
            for n, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
                if BUILT.search(line):
                    hits.append("%s:%d" % (f.relative_to(ROOT), n))
        self.assertEqual(hits, [])


if __name__ == "__main__":
    unittest.main()
