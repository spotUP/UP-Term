"""upkg_browse_rig.setup(): the old base's Delete needs no program running from its bin.

The image's User-Startup runs UP-Term:bin/Amarchy, which starts amarchy-gui and amarchy-menu; each holds a lock on the
bin drawer and the Delete failed with "bin Not Deleted: object is in use" (rc 20, 2026-10-29, rig 10).
"""
import pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import upkg_browse_rig as ub
import upkg_rig as ur

STATUS = """Process  1: Loaded as command: DO_LAUNCHER
Process  2: Loaded as command: BOOTX:wasabid
Process  5: Loaded as command: UP-Term:bin/Amarchy
Process  6: Loaded as command: Status
Process  7: Loaded as command: Rushhours:Tools/UP-Term/bin/amarchy-gui
Process  8: Loaded as command: Rushhours:Tools/UP-Term/bin/amarchy-menu
Process  9: Loaded as command: DOpus5:DirectoryOpus
"""


class StopBaseTest(unittest.TestCase):
    def test_finds_only_programs_from_the_bin(self):
        self.assertEqual(ur.base_processes(STATUS), [5, 7, 8])

    def test_breaks_until_none_is_left(self):
        left = [STATUS, "Process  6: Loaded as command: Status\n"]
        sent = []
        def run(cmd):
            if cmd == 'Status':
                return 0, left.pop(0)
            sent.append(cmd)
            return 0, ''
        with mock.patch.object(ub.ir, 'run', run), mock.patch.object(ur.time, 'sleep'):
            ur.stop_base_programs()
        self.assertEqual(sent, ['Break 5 ALL', 'Break 7 ALL', 'Break 8 ALL'])

    def test_setup_stops_them_before_the_delete(self):
        kit = pathlib.Path(tempfile.mkdtemp())
        (kit / 'Files').mkdir()
        (kit / 'Files/VERSIONS').write_text('x')
        order = []
        def run(cmd, *a):
            order.append(cmd)
            return (1, '') if 'EXISTS' in cmd else (0, '')
        with mock.patch.object(ub.ir, 'run', run), mock.patch.object(ub.ir, 'put_vsh'), mock.patch.object(ub.ir, 'prepare'), \
                mock.patch.object(ur, 'stop_base_programs', lambda: order.append('STOP')), \
                mock.patch.object(ub.ir, 'run_slow', lambda n, c, t: (order.append(c), (0, ''))[1]), \
                mock.patch.object(ur, 'base_stale', return_value=True), mock.patch.object(ur, 'free_check', side_effect=SystemExit('past delete')), \
                mock.patch.object(ub.ir, 'KIT', kit):
            with self.assertRaises(SystemExit):
                ub.setup(mock.Mock(), ub.State())
        deletes = [i for i, c in enumerate(order) if c.startswith('Delete ') and 'ALL QUIET FORCE' in c]
        self.assertTrue(deletes and order.index('STOP') < deletes[0], order)


if __name__ == '__main__':
    unittest.main()
