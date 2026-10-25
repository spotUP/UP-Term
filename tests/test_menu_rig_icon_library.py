"""app_icons needs icon.library V44 (icon_gfx.c ig_open). On the default KS 3.1 rig (icon.library 40) the
drawn menu keeps the generic glyph and ENV:AMARCHY_ICON_LOADS stays 0, which showed as 3 of 10 failed checks
that looked like a drawing bug (and were blamed on commit 51e0abfd). The row now names the missing library."""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_menu_rig as m


class IconLibrary(unittest.TestCase):
    def major(self, rc, out):
        with mock.patch.object(m.c, "run", return_value=(rc, out)):
            return m.icon_library_major()

    def test_versions(self):
        self.assertEqual(self.major(0, "icon.library 40.1\n"), 40)
        self.assertEqual(self.major(0, "icon.library 47.4\n"), 47)
        self.assertEqual(self.major(10, ""), 0)

    def test_row_stops_below_v44(self):
        with mock.patch.object(m, "ensure_terminal"), mock.patch.object(m.c, "run", return_value=(0, "icon.library 40.1")):
            ok, msg = m.r_app_icons()
        self.assertFalse(ok)
        self.assertIn("V44", msg)


class Os32Rig(unittest.TestCase):
    def test_flag_forms(self):
        self.assertTrue(m.is_os32(['--max', '--os32']))
        self.assertTrue(m.is_os32(['--os', '322', '--max']))
        self.assertFalse(m.is_os32(['--max']))
        self.assertFalse(m.is_os32(['--os', '31']))

    def test_start_args_switch_to_32_keep_flags(self):
        a = m.os32_start_args(['--max', '--os', '31', '--purpose', 'x: y'])
        self.assertEqual(a, ['--max', '--purpose', 'x: y', '--os', '32'])
        self.assertTrue(m.is_os32(a))

    def test_rig_restarted_on_os32(self):
        calls = []
        seq = [['--max'], ['--max', '--os', '32']]
        with mock.patch.object(m, "start_args", side_effect=lambda: seq.pop(0)), \
             mock.patch.object(m.paths, "RIG_N", 8), \
             mock.patch.object(m.subprocess, "run", side_effect=lambda cmd, **k: calls.append(cmd[3:])):
            self.assertEqual(m.ensure_os32_rig(), '')
        self.assertEqual(calls, [['stop'], ['start', '--max', '--os', '32']])

    def test_os32_rig_left_alone_and_rig1_never_stopped(self):
        with mock.patch.object(m, "start_args", return_value=['--os32']), mock.patch.object(m.subprocess, "run") as r:
            self.assertEqual(m.ensure_os32_rig(), '')
            r.assert_not_called()
        with mock.patch.object(m, "start_args", return_value=['--max']), mock.patch.object(m.paths, "RIG_N", 1), \
             mock.patch.object(m.subprocess, "run") as r:
            self.assertIn('rig 1', m.ensure_os32_rig())
            r.assert_not_called()


if __name__ == "__main__":
    unittest.main()
