"""A fresh rig dir is start-ready without a hand copy (RIG-SCREENMODE-PREFS-UNSTAGED, 2026-10-27).

rig.py stage_rig_vtc(vtc) decides what a rig's VTC: drawer needs; setup() calls it for build/rig and build/rigN
alike. No emulator runs: a temp drawer is staged and checked for the ScreenMode prefs (os_image_copy and
`rig.py aga|aga256|rtg` read them) and for the UP-Term: drawer the boot's UP-Term: assign points at (VTC:Apps,
with bin/vsh). The prefs are compared byte for byte with the files rig 1 holds.
"""
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
saved = sys.argv
sys.argv = ["rig.py", "test"]
import rig  # noqa: E402
sys.argv = saved

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


with tempfile.TemporaryDirectory() as d:
    vtc = pathlib.Path(d) / "rig10" / "vtc"
    vtc.mkdir(parents=True)
    check(hasattr(rig, "stage_rig_vtc"), "rig.stage_rig_vtc exists")
    if hasattr(rig, "stage_rig_vtc"):
        rig.stage_rig_vtc(vtc)
        for name in ("aga16", "aga256", "rtg", "rtg360"):
            f = vtc / ("screenmode.%s.prefs" % name)
            check(f.is_file() and f.read_bytes()[:4] == b"FORM" and len(f.read_bytes()) == 62, "screenmode.%s.prefs staged" % name)
            ref = rig.paths.RIG_HOME / "rig/vtc" / f.name
            if ref.is_file():
                check(f.read_bytes() == ref.read_bytes(), "%s equals rig 1's" % f.name)
        check((vtc / rig.paths.UPTERM_DRAWER / "bin/vsh").is_file(), "UP-Term drawer has bin/vsh (UP-Term: assign target)")
        check((vtc / rig.paths.UPTERM_DRAWER / "UP-Term.info").is_file(), "UP-Term drawer has its icon")
        (vtc / "screenmode.aga256.prefs").write_bytes(b"mine")
        rig.stage_rig_vtc(vtc)
        check((vtc / "screenmode.aga256.prefs").read_bytes() == b"mine", "a second staging keeps an existing file")

check("C:Assign >NIL: TMP: T:" in rig.GO, "the boot script assigns TMP: (/tmp) as the kit does")

print("FAIL" if fails else "PASS: test_rig_stage")
sys.exit(1 if fails else 0)
