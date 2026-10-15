"""tools/rig/claude_rig.py: importing it must not start a rig or run the test.

Regression (2026-10-15): claude_rig.py ended in a bare main(), so `import claude_rig` ran the whole
rig test against whatever rig was up. Importing must only define; running it as a script still runs.
"""
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
RIG = HERE / "rig"
r = subprocess.run([sys.executable, "-I", "-c",
    "import sys; sys.path.insert(0, %r); import claude_rig; "
    "assert callable(claude_rig.main); print('IMPORTED')" % str(RIG)],
    capture_output=True, text=True, timeout=30)
if r.returncode != 0 or r.stdout.strip() != "IMPORTED":
    sys.exit("FAIL: importing tools/rig/claude_rig.py ran something (rc=%d): %s%s"
             % (r.returncode, r.stdout.strip()[-200:], r.stderr.strip()[-200:]))
print("[OK] rig_claude_import: 1 check, 0 failed")
