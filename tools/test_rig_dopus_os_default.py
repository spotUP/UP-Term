"""`rig.py dopus` with no system argument installs on the system the rig was started with (RIG-DOPUS-OS-DEFAULT).

dopus_rig.system_args() chose the system from argv alone: a rig started with --os 32 got the install on a sys.hdf clone
and the eye-check still reported DOpus missing in the os-32 tree. No emulator runs: a fake rig dir holds a START_ARGS
record and rig.OS / rig.OS32 stand for the argv of the dopus command.
"""
import json
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
saved = sys.argv
sys.argv = ["rig.py", "dopus"]
import rig  # noqa: E402
import dopus_rig  # noqa: E402
sys.argv = saved

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


with tempfile.TemporaryDirectory() as d:
    rec = pathlib.Path(d) / "start.args"
    rig.START_ARGS = rec
    rec.write_text(json.dumps(["--max", "--os", "32", "--purpose", "x"]))
    rig.OS, rig.OS32 = None, False
    check(dopus_rig.system_args() == ["--os", "32"], "record --os 32, argv names none: %s" % dopus_rig.system_args())
    check(dopus_rig.target(dopus_rig.system_args())[0].startswith("os-32 tree "), "target line names the os-32 tree")
    rig.OS, rig.OS32 = None, True
    check(dopus_rig.system_args() == ["--os32"], "argv --os32 wins over the record: %s" % dopus_rig.system_args())
    rig.OS, rig.OS32 = None, False
    rec.unlink()
    check(dopus_rig.system_args() == [], "no record, no argv: the sys.hdf clone")
    check(dopus_rig.target([]) == ("sys.hdf clone", None), "target line says sys.hdf clone")

sys.exit(1 if fails else 0)
