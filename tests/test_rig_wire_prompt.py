"""The rigs that log in over the serial wire wait for vsh's prompt with one pattern.

zmodem_rig kept its own '[%#$>] $' after vsh began ending every prompt with
the OSC 133 B marker (6318b95), so its first check failed: "prompt False"
(2026-10-25). getty_rig.PROMPT is now the one pattern; no rig keeps a copy.
"""
import ast, pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import getty_rig

WIRE_RIGS = ("getty_rig.py", "zmodem_rig.py")


class WirePrompt(unittest.TestCase):
    def test_pattern_matches_the_marker_vsh_writes(self):
        src = (ROOT / "shell/vsh.c").read_text(errors="replace")
        self.assertIn("133;B", src, "vsh no longer writes OSC 133 B: update getty_rig.PROMPT")
        self.assertTrue(re.search(getty_rig.PROMPT, b"Work:> \x1b]133;B\x07"))
        self.assertFalse(re.search(getty_rig.PROMPT, b"Work:> "))

    def test_no_rig_keeps_its_own_prompt_literal(self):
        for name in WIRE_RIGS:
            tree = ast.parse((ROOT / "tools/rig" / name).read_text())
            for node in ast.walk(tree):
                if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "prompt" for t in node.targets):
                    self.assertNotIsInstance(node.value, ast.Constant,
                                             "%s:%d sets prompt to a literal; use getty_rig.PROMPT" % (name, node.lineno))


if __name__ == "__main__":
    unittest.main()
