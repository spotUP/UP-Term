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

ASSIGNS = "Volumes:\nWork [Mounted]\nEmpty-12MB-FFS [Mounted]\n\nDirectories:\nSYS  Empty-12MB-FFS:\nC    Empty-12MB-FFS:C\n"


def fake(dir_text):
    def run(cmd, *a):
        if cmd == 'Type ENVARC:up-term/Dir':
            return (0, dir_text) if dir_text is not None else (205, '')
        if cmd == 'Assign LIST':
            return 0, ASSIGNS
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


if __name__ == '__main__':
    unittest.main()
