"""tools/rig/install_rig.py: a run refuses up front, naming `make rig-helpers`, when a helper program is not built.

Regression (s15 2026-10-28): the two-directory UP-Term re-proof on rig 10 ran from a fresh
worktree after `make dist-base`. prepare() had already replaced VTC:distkit when
shutil.copyfile raised FileNotFoundError for build/amiga/iconprobe: dist-base does not build
iconprobe, wbrun or conwho, and nothing said which make line does.
"""
import pathlib
import re
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import install_rig as ir  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


with tempfile.TemporaryDirectory() as t:
    root = pathlib.Path(t)
    (root / "build/amiga").mkdir(parents=True)
    for n in ("ptytest", "UPConsole"):  # what make dist-base leaves
        (root / "build/amiga" / n).write_bytes(b"\0")
    msg = ir.missing_helpers(root) or ""
    check("make rig-helpers" in msg, "the refusal names make rig-helpers: %r" % msg)
    check(all(n in msg for n in ("iconprobe", "wbrun", "conwho")), "the refusal names each missing helper: %r" % msg)
    check("ptytest" not in msg, "a built helper is not named: %r" % msg)
    for n in ir.HELPERS:
        (root / "build/amiga" / n).write_bytes(b"\0")
    check(ir.missing_helpers(root) is None, "all helpers built: no refusal")

# wired: prepare(), the entry of every install_rig run, refuses before VTC:distkit changes
with tempfile.TemporaryDirectory() as t:
    root = pathlib.Path(t)
    (root / "build/amiga").mkdir(parents=True)
    (root / "vtc/distkit").mkdir(parents=True)
    (root / "vtc/distkit/sentinel").write_text("old kit")
    ir.ROOT, ir.VTC = root, root / "vtc"
    try:
        ir.prepare(None)
        check(False, "prepare() without helpers refuses")
    except SystemExit as e:
        check("make rig-helpers" in str(e), "prepare() refusal names make rig-helpers: %r" % e)
    check((root / "vtc/distkit/sentinel").exists(), "prepare() refused before replacing VTC:distkit")

# the make target builds exactly the set prepare() copies
mk = (HERE.parent / "Makefile").read_text()
m = re.search(r"^rig-helpers:(.*)$", mk, re.M)
built = set(re.findall(r"\$\(BUILD\)/amiga/(\S+)", m.group(1))) if m else set()
check(built == set(ir.HELPERS), "Makefile rig-helpers %s equals install_rig.HELPERS %s" % (sorted(built), sorted(ir.HELPERS)))

print("rig_helpers: %s" % ("FAIL" if fails else "PASS"))
sys.exit(1 if fails else 0)
