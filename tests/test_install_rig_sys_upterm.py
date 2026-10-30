"""install_rig's base Uninstall check: SYS:UP-Term must be as it was before Install, not absent.

The master image nyhd2.hdf carries an empty DH0:UP-Term (dated 2026-10-11) that no Install made. Uninstall rightly
leaves it, and the old check "SYS:UP-Term is gone" failed every installer_rig run with 'Directory "SYS:UP-Term" is
empty' (ledger row 2365, 2026-10-29).
"""
import pathlib, sys, types, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import install_rig as ir

EMPTY = (0, 'Directory "SYS:UP-Term" is empty\n')
ABSENT = (205, "Can't examine \"SYS:UP-Term\": object not found\n")
KIT = (0, "VERSIONS  120  ----rwed\n")


class Checks:
    def __init__(self):
        self.results = {}

    def __call__(self, ok, what, detail=""):
        self.results[what] = bool(ok)


def uninstall(before_sys, after_sys):
    before = types.SimpleNamespace(startup="", sseq="seq", libs={}, sys_upterm=before_sys)
    def run(line):
        if line == 'List SYS:UP-Term ALL':
            return after_sys
        if line == 'Type S:Startup-Sequence':
            return 0, "seq"
        return 5, ""
    ck = Checks()
    with mock.patch.object(ir, "run_long", return_value=(0, "")), mock.patch.object(ir, "run", run), \
            mock.patch.object(ir, "lib_state", return_value={}):
        ir.base_uninstall(ck, before, expect_user_startup_clean=False)
    return ck.results['after base Uninstall: SYS:UP-Term as it was before Install']


class SysUpTerm(unittest.TestCase):
    def test_empty_drawer_from_the_image_left_as_it_was_passes(self):
        self.assertTrue(uninstall(EMPTY, EMPTY))

    def test_absent_before_and_after_passes(self):
        self.assertTrue(uninstall(ABSENT, ABSENT))

    def test_kit_files_left_behind_fail(self):
        self.assertFalse(uninstall(EMPTY, KIT))

    def test_drawer_made_by_the_install_and_left_fails(self):
        self.assertFalse(uninstall(ABSENT, EMPTY))


def dest_check(before_sys, after_sys):
    """base-dest: an Install given DEST=VTC:KitTest/UP-Term, then SYS:UP-Term listed again."""
    ck = Checks()
    with mock.patch.object(ir, "run", lambda line: after_sys if line == 'List SYS:UP-Term ALL' else (5, "")):
        ir.check_dest_leaves_sys_upterm(ck, before_sys)
    return ck.results['a non-default DEST leaves SYS:UP-Term as it was']


class DestLeavesSysUpTerm(unittest.TestCase):
    """BASEDEST-EMPTY-SYSUPTERM (2026-10-30): base-dest FAIL 7 on a fresh rig 3 was the image's empty drawer."""
    def test_empty_drawer_from_the_image_left_as_it_was_passes(self):
        self.assertTrue(dest_check(EMPTY, EMPTY))

    def test_absent_before_and_after_passes(self):
        self.assertTrue(dest_check(ABSENT, ABSENT))

    def test_install_made_the_drawer_fails(self):
        self.assertFalse(dest_check(ABSENT, EMPTY))

    def test_install_copied_into_the_drawer_fails(self):
        self.assertFalse(dest_check(EMPTY, KIT))


if __name__ == "__main__":
    unittest.main()
