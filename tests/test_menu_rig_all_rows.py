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
        with mock.patch.object(m, 'tx_all_rows', lambda: screens.pop(0)), mock.patch.object(m.ami, 'key', keys.append), \
                mock.patch.object(m.time, 'sleep'):
            got = m.tx_list_after_job('up-pi')
        self.assertEqual(got, ['[UHC] up-pi  Installed   >'])
        self.assertEqual(keys, [m.RAW['esc']])

    def test_list_after_job_finds_a_row_below_the_window(self):
        """r3 on rig 4: after the Install job the list is back at its top; the 3-row window shows row0..row2 and the
        package sits at row5. The window alone finds no row (the old IndexError); the whole list does, with no Escape."""
        items = ['row%d' % i for i in range(5)] + ['[UHC] up-pi  Installed   >', 'row6']
        st = {'cur': 0, 'keys': []}

        def key(k):
            st['keys'].append(k)
            st['cur'] = min(6, st['cur'] + 1) if k == m.RAW['down'] else max(0, st['cur'] - 1)

        def rows():
            top = max(0, min(st['cur'] - 2, 4))
            return items[top:top + 3]

        with mock.patch.object(m, 'tx_rows', rows), mock.patch.object(m.ami, 'key', key), mock.patch.object(m.time, 'sleep'):
            got = m.tx_list_after_job('up-pi')
        self.assertEqual(len(m.tx_named(got, 'up-pi')), 1)
        self.assertNotIn(m.RAW['esc'], st['keys'])


if __name__ == '__main__':
    unittest.main()
