#!/usr/bin/env python3
"""No rig script keeps its output under paths.RIG (build/rig<N>): `rig.py stop` deletes that folder for N > 1
(commit 8df99f0b), so reports, screenshots and verdicts written there vanished with the rig (s50). Kept output
lives under paths.REPORTS. Parses every tools/rig/*.py with ast: a module-level OUT / SHOT(S) / VERDICTS name, or an
argparse --out default, built from RIG fails."""
import ast, pathlib, re, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
RIG_DIR = ROOT / "tools/rig"
sys.path.insert(0, str(RIG_DIR))
import paths

KEPT = re.compile(r"^(OUT\w*|SHOTS?\w*|VERDICTS?\w*|REPORTS?\w*|EVIDENCE\w*)$")


def uses_rig(node):
    """True when the expression reads paths.RIG or a bare RIG."""
    for n in ast.walk(node):
        if isinstance(n, ast.Attribute) and n.attr == "RIG" and isinstance(n.value, ast.Name) and n.value.id == "paths":
            return True
        if isinstance(n, ast.Name) and n.id == "RIG":
            return True
    return False


def offenders():
    bad = []
    for f in sorted(RIG_DIR.glob("*.py")):
        if f.name == "paths.py":
            continue
        tree = ast.parse(f.read_text(encoding="utf-8"), str(f))
        for n in ast.walk(tree):
            if isinstance(n, ast.Assign) and uses_rig(n.value):
                for t in n.targets:
                    if isinstance(t, ast.Name) and KEPT.match(t.id) and n in tree.body:
                        bad.append("%s:%d %s" % (f.name, n.lineno, t.id))
            if isinstance(n, ast.Call) and getattr(n.func, "attr", "") == "add_argument":
                opts = [a.value for a in n.args if isinstance(a, ast.Constant)]
                if any(re.match(r"--(out|output|report|shots)", str(o)) for o in opts):
                    for kw in n.keywords:
                        if kw.arg == "default" and uses_rig(kw.value):
                            bad.append("%s:%d %s default" % (f.name, n.lineno, opts[0]))
    return bad


class ReportsOutsideRigDir(unittest.TestCase):
    def test_no_script_keeps_output_under_the_disposable_rig_folder(self):
        self.assertEqual(offenders(), [])

    def test_reports_dir_is_per_rig_and_not_inside_the_rig_folder(self):
        self.assertEqual(paths.REPORTS.name, paths.RIG_NAME)
        self.assertNotIn(paths.RIG, paths.REPORTS.parents)
        self.assertNotEqual(paths.REPORTS, paths.RIG)


if __name__ == "__main__":
    unittest.main()
