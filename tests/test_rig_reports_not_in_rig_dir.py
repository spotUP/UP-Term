#!/usr/bin/env python3
"""No rig script keeps its output under paths.RIG (build/rig<N>): `rig.py stop` deletes that folder for N > 1
(commit 8df99f0b), so reports, screenshots and verdicts written there vanished with the rig (s50). Kept output
lives under paths.REPORTS. Parses every tools/rig/*.py with ast: a module-level OUT / SHOT(S) / VERDICTS / LOG* / DUMP*
name, an argparse --out default, or any statement that joins RIG with a file name a human or a later run reads (a
string ending .txt/.md/.json/.jsonl/.png/.rgb/.log, or naming a dump / verdict directory) fails, unless it is in
SCRATCH below. Also: paths.reports_dir() creates the directory it returns."""
import ast, pathlib, re, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
RIG_DIR = ROOT / "tools/rig"
sys.path.insert(0, str(RIG_DIR))
import paths

KEPT = re.compile(r"^(OUT\w*|SHOTS?\w*|VERDICTS?\w*|REPORTS?\w*|EVIDENCE\w*|LOG\w*|DUMP\w*|FAILS|LEDGER|TIMING)$")
# a string literal joined to RIG that names kept output: a file a human or a later run reads, or a dump / verdict dir
KEPT_LITERAL = re.compile(r"\.(txt|md|json|jsonl|png|rgb|log)$|dump|^(splits|install|userland)$")
# Real emulator-owned scratch that stays under RIG (the rig's own state, gone with the rig on purpose). Each entry is
# a string literal; the reason is the comment.
SCRATCH = {
    "serial.log", "debugger.log", "boot.log", "retry.log", "watchdog.log",  # FS-UAE / rig.py start-up and serial wire logs
    "requesters.log",       # ami.py: system requesters seen during this boot
    "preinstalled.json",    # install_rig: put-back state that must survive a crash inside the SAME rig, not a report
    "hlmd-doc.md",          # hlmd_rig: fixture copied onto the Amiga disk, an input not an output
    "RAM:README.md",        # hlmd_rig: the Amiga-side destination of that fixture, not a host path
    "boot/boot.log", "fs-uae.log.txt",   # rig.py: the emulator's own boot and FS-UAE logs
    "owner.json",           # rig.py OWNER_FILE: who started this rig, read by mine()/stop; rig state, gone with the rig
    "vt/charset.txt",       # vttest_rig: payload written into the Amiga disk image (vtc/)
}


def uses_rig(node):
    """True when the expression reads paths.RIG or a bare RIG."""
    for n in ast.walk(node):
        if isinstance(n, ast.Attribute) and n.attr == "RIG" and isinstance(n.value, ast.Name) and n.value.id == "paths":
            return True
        if isinstance(n, ast.Name) and n.id == "RIG":
            return True
    return False


def kept_literals(stmt):
    return [n.value for n in ast.walk(stmt) if isinstance(n, ast.Constant) and isinstance(n.value, str)
            and KEPT_LITERAL.search(n.value) and n.value not in SCRATCH and not n.value.startswith("vtc/")]


def offenders():
    bad = []
    for f in sorted(RIG_DIR.glob("*.py")):
        if f.name == "paths.py":
            continue
        tree = ast.parse(f.read_text(encoding="utf-8"), str(f))
        for n in ast.walk(tree):
            if isinstance(n, ast.stmt) and not hasattr(n, "body") and uses_rig(n):
                for lit in kept_literals(n):
                    bad.append("%s:%d %r" % (f.name, n.lineno, lit))
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


class ReportsDir(unittest.TestCase):
    def test_reports_dir_creates_what_it_returns(self):
        root = pathlib.Path(tempfile.mkdtemp())
        old = paths.REPORTS
        paths.REPORTS = root / "rig-reports" / "rig9"
        try:
            self.assertEqual(paths.reports_dir("shots"), root / "rig-reports/rig9/shots")
            self.assertTrue((root / "rig-reports/rig9/shots").is_dir())
            self.assertEqual(paths.reports_dir(), root / "rig-reports/rig9")
            paths.reports_dir("shots")   # twice: no error
        finally:
            paths.REPORTS = old


if __name__ == "__main__":
    unittest.main()
