"""prof68k.py profiles the painter on the colour shapes (ASM1, 2026-10-16).

Builds tests/amiga/paintprof.c with the vbcc cfg of prof68k.py, runs 20 rows of
each shape under vamos through prof68k.py's own entry point and reads the
per-function table: the painter's functions must be in it (proof the new code
ran), the pen-1-on-0 one-plane shape must reach vp_asm_plane.

    python3 -m pytest -q tools/test_prof68k_paint.py
"""
import pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROF = ROOT / "tools/prof68k.py"


def prof(*args):
    r = subprocess.run([sys.executable, str(PROF)] + list(args), cwd=ROOT, capture_output=True, text=True, timeout=900)
    assert r.returncode == 0, r.stderr[-2000:]
    return r.stdout


def table(out):
    """function -> instructions, from the per-function table."""
    rows = {}
    for l in out.split("\nhottest lines:")[0].splitlines():
        p = l.split()
        if len(p) == 4 and p[1].isdigit():
            rows[p[0]] = int(p[1])
    return rows


def test_painter_functions_counted_on_each_shape():
    prof("build-paint")
    for shape, planes in (("0", "3"), ("1", "3"), ("2", "1")):
        rows = table(prof("run-paint", shape, planes, "20"))
        assert any(f.startswith("painter:vp_span") for f in rows), (shape, rows)
        assert rows.get("asm:vp_asm_plane", 0) > 0, (shape, "vp_asm_plane did not run", rows)
        assert sum(rows.values()) > 20 * 100, (shape, rows)
