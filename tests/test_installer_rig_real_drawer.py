"""installer_rig judges the drawer the Installer really chose (ENVARC:up-term/Dir), not UPBigVol's separate prediction.

On OS 3.2 rigs every volume reports equal free space; the Installer's tie-break gave SYS: while a separate UPBigVol run
predicted Work:, so checks 4, 5, 8, 11 expected Work:UP-Term and check 10 compared absent-before with
fully-installed SYS:UP-Term. SYS: is also reachable under its volume name (Empty-12MB-FFS:UP-Term).
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import installer_rig as rig

CD_OUT = "Empty-12MB-FFS:\n"   # what a bare CD prints after CD SYS:


def fake(dir_text, choice=None, cd_out=CD_OUT):
    def run(cmd, *a):
        if cmd == 'CD SYS:\nCD':
            return 0, cd_out
        if cmd.startswith('Type ') and 'UPTOPTDEST' in cmd:
            return (0, choice) if choice is not None and cmd.startswith('Type ENVARC') else (205, '')
        if cmd == 'Type ENVARC:up-term/Dir':
            return (0, dir_text) if dir_text is not None else (205, '')
        raise AssertionError(cmd)
    return run


def gate(dir_text, predicted='Work:UP-Term'):
    """The drawer the checks use and whether check 10 runs, with UPBigVol predicting `predicted`."""
    with mock.patch.object(rig.ir, 'run', fake(dir_text)):
        drawer = rig.real_drawer() or predicted
        return drawer, not rig.on_sys_volume(drawer)


class RealDrawer(unittest.TestCase):
    def test_installer_chose_sys_though_prediction_was_work_skips_check_10(self):
        self.assertEqual(gate('SYS:UP-Term\n'), ('SYS:UP-Term', False))

    def test_sys_under_its_volume_name_skips_check_10(self):
        self.assertEqual(gate('Empty-12MB-FFS:UP-Term\n'), ('Empty-12MB-FFS:UP-Term', False))

    def test_installer_chose_work_runs_check_10(self):
        self.assertEqual(gate('Work:UP-Term\n', predicted='SYS:UP-Term'), ('Work:UP-Term', True))

    def test_no_recorded_dir_falls_back_to_the_prediction(self):
        self.assertEqual(gate(None)[0], 'Work:UP-Term')


def check5(dir_text, choice):
    """Check 5's verdict: ENVARC:up-term/Dir against the Installer's own record (UPTOPTDEST)."""
    with mock.patch.object(rig.ir, 'run', fake(dir_text, choice)):
        return rig.same_drawer_volume(dir_text.strip(), rig.installer_choice())


class Check5Independent(unittest.TestCase):
    def test_dir_names_work_but_installer_chose_sys_volume_fails(self):
        self.assertFalse(check5('Work:UP-Term\n', 'Empty-12MB-FFS:UP-Term\n'))

    def test_sys_and_its_volume_name_are_one_volume(self):
        self.assertTrue(check5('SYS:UP-Term\n', 'Empty-12MB-FFS:UP-Term\n'))

    def test_same_name_different_case_matches(self):
        self.assertTrue(check5('empty-12mb-ffs:UP-Term\n', 'Empty-12MB-FFS:UP-Term\n'))

    def test_no_installer_record_is_none(self):
        with mock.patch.object(rig.ir, 'run', fake('Work:UP-Term\n', None)):
            self.assertIsNone(rig.installer_choice())


class SysVolumeName(unittest.TestCase):
    def test_parses_bare_cd_output(self):
        with mock.patch.object(rig.ir, 'run', fake(None)):
            self.assertEqual(rig.sys_volume_name(), 'EMPTY-12MB-FFS')

    def test_tolerates_blank_lines_and_spaces(self):
        with mock.patch.object(rig.ir, 'run', fake(None, cd_out="\n  Work:  \n")):
            self.assertEqual(rig.sys_volume_name(), 'WORK')


if __name__ == '__main__':
    unittest.main()
