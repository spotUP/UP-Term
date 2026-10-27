"""tx_all_rows lists a level longer than the text menu window (PG.8: Browse must count 28 rows, the window shows 10)."""
import pathlib, sys, unittest
from unittest import mock
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'tools' / 'rig'))
import amarchy_menu_rig as m


class AllRows(unittest.TestCase):
    def test_scrolls_to_every_row(self):
        items = ['row%d' % i for i in range(7)]
        st = {'cur': 0, 'keys': []}

        def key(k):
            st['keys'].append(k)
            st['cur'] = min(6, st['cur'] + 1) if k == m.RAW['down'] else max(0, st['cur'] - 1)

        def rows():
            top = max(0, min(st['cur'] - 2, 4))
            return items[top:top + 3]

        with mock.patch.object(m, 'tx_rows', rows), mock.patch.object(m.ami, 'key', key), mock.patch.object(m.time, 'sleep'):
            got = m.tx_all_rows()
        self.assertEqual(got, items)
        self.assertEqual(st['cur'], 0)

    def test_named_is_a_whole_word(self):
        rows = ['[upkg] up-pi  1.0   >', '[upkg] up-picocom  1.0   >', '[UHC] up-pi  Installed']
        self.assertEqual(m.tx_named(rows, 'up-pi'), [rows[0], rows[2]])

    def test_list_after_job_goes_up_from_the_action_level(self):
        screens = [['Remove'], ['[UHC] up-pi  Installed   >']]
        keys = []
        with mock.patch.object(m, 'tx_rows', lambda: screens.pop(0)), mock.patch.object(m.ami, 'key', keys.append), \
                mock.patch.object(m.time, 'sleep'):
            got = m.tx_list_after_job('up-pi')
        self.assertEqual(got, ['[UHC] up-pi  Installed   >'])
        self.assertEqual(keys, [m.RAW['esc']])


if __name__ == '__main__':
    unittest.main()
