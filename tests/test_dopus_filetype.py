#!/usr/bin/env python3
"""The Directory Opus filetype file tools/gen_dopus_filetype.py writes (plan 2026-10-25-amarchy-dopus-integration.md, D11):
the bytes are golden; and, when the upstream dopus5 v5.104 source is at $DOPUS5_SRC, its archive/tools/filetype_tool.py
`check` (decode and re-encode equal) passes and `lint` reports nothing but the UP-TERM: assign (ours, not a stock
one) and the note that the type is recognised by file name only. The tool is run with -I, from another directory."""
import os, pathlib, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_dopus_filetype as g

GOLDEN = bytes.fromhex(
    "464f524d000000ee4f505553545950450000005555502d5465726d0000000000"
    "000000000000000000000000000000000000000055505445524d00000000f600"
    "001002233f2e28637c687c7478747c6d647c7265787829000000000000000000"
    "0000000000000000000046554e430000003e000000100000000000000000ffff"
    "00000002ffff000052756e203e4e494c3a2055502d5465726d3a62696e2f7574"
    "65726d2d65646974202225732200000146554e430000003e0000001000000000"
    "00000000ffff00000000ffff000052756e203e4e494c3a2055502d5465726d3a"
    "62696e2f757465726d2d766965772022257322000001"
)
SRC = os.environ.get("DOPUS5_SRC", "")
TOOL = pathlib.Path(SRC) / "archive/tools/filetype_tool.py"


class Filetype(unittest.TestCase):
    def test_the_bytes_are_the_golden_file(self):
        self.assertEqual(g.generate(), GOLDEN)

    def test_a_double_click_runs_the_editor_and_user1_the_viewer(self):
        data = g.generate()
        self.assertIn(b'Run >NIL: UP-Term:bin/uterm-edit "%s"', data)
        self.assertIn(b'Run >NIL: UP-Term:bin/uterm-view "%s"', data)
        self.assertTrue(data.startswith(b"FORM") and data[8:12] == b"OPUS")

    @unittest.skipUnless(TOOL.is_file(), "set DOPUS5_SRC to a dopus5 v5.104 checkout")
    def test_upstream_check_round_trips_and_lint_is_clean(self):
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / "UP-Term"
            f.write_bytes(g.generate())
            run = lambda cmd: subprocess.run([sys.executable, "-I", str(TOOL), cmd, str(f)], cwd=d,
                                             capture_output=True, text=True)
            c = run("check")
            self.assertEqual((c.returncode, c.stdout, c.stderr), (0, "", ""))
            l = run("lint")
            self.assertEqual(l.stderr, "")
            rest = [x for x in l.stdout.splitlines()
                    if "needs UP-TERM:" not in x and "recognised by file name only" not in x]
            self.assertEqual(rest, [])


if __name__ == "__main__":
    unittest.main()
