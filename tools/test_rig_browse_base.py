"""tools/rig/upkg_browse_rig.py: step 0 installs the kit over an older base on the rig.

Regression (2026-10-21, artprobe on rig 2): step 0 installed the base only when the rig had no
UP-Term:, so a rig still carrying the 1.3 base ran `upkg browse` against the old vtcon handler and
the logo-overlap fix (e7a4aeb9) looked absent until the install was forced by hand.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import upkg_browse_rig as b  # noqa: E402

fails = 0
checks = 0


def check(cond, what):
    global fails, checks
    checks += 1
    if not cond:
        fails += 1
        print("FAIL:", what)


KIT = "UP-Term 1.4 e7a4aeb9 2026-10-21 (upterm 5f51b85)\nvtcon main e7a4aeb9\n"
OLD = "UP-Term 1.3 3d11431 2026-10-12 (upterm 1a2b3c4)\nvtcon main 3d11431\n"
check(b.base_stale(None, KIT), "no UP-Term:VERSIONS: the kit is installed")
check(b.base_stale("", KIT), "an empty UP-Term:VERSIONS: the kit is installed")
check(b.base_stale(OLD, KIT), "an older base: the kit is installed over it")
check(b.base_stale(KIT.replace("e7a4aeb9", "ba16e0c4", 1), KIT), "the same release from another commit: the kit is installed")
check(not b.base_stale(KIT, KIT), "the kit's own base: no install")
check(not b.base_stale("\n" + KIT.replace("\n", "\r\n"), KIT), "Type's line ends and a leading blank do not force an install")

# step 0 gates the install on base_stale, not on the UP-Term: assign alone
src = (HERE / "rig/upkg_browse_rig.py").read_text()
check("if base_stale(installed, kit_versions.read_text()):" in src, "step 0 installs when base_stale says so")
check("if ir.run('Assign >NIL: UP-Term: EXISTS')[0] != 0:" not in src, "step 0 no longer skips the install whenever UP-Term: exists")

print("[%s] rig_browse_base: %d checks, %d failed" % ("OK" if not fails else "ERROR", checks, fails))
sys.exit(1 if fails else 0)
