#!/usr/bin/env python3
"""INSTALLER-CHECK10: Uninstall deletes the UP-Term drawer only when Install made it.

An empty SYS:UP-Term on the master image was gone after Install + Uninstall. Install now writes
ENVARC:up-term/DirWasThere when the drawer existed before it and the Dir record did not already name it;
Uninstall then empties the drawer and leaves it."""
import pathlib, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
INSTALL = (ROOT / 'dist/install.dos').read_text(encoding='latin-1')
UNINSTALL = (ROOT / 'dist/Uninstall').read_text(encoding='latin-1')
EMPTY = 'Delete >NIL: "$updest/#?" ALL QUIET'
WHOLE = 'Delete >NIL: "$updest" ALL QUIET'


class OldDrawerSurvives(unittest.TestCase):
    def test_install_records_a_drawer_that_was_already_there_before_it_makes_it(self):
        pre = INSTALL[:INSTALL.index('MakeDir >NIL: "$updest"')]
        rec = pre.index('Echo >ENVARC:up-term/DirWasThere "$updest"')
        self.assertIn('If EXISTS "$updest"\n  Set updirthere YES', pre)
        # an Install over our own Install (Dir already names the drawer) keeps the record as it is
        self.assertIn('Search >NIL: ENVARC:up-term/Dir "$updest"', pre)
        self.assertIn('Delete >NIL: ENVARC:up-term/DirWasThere QUIET', pre)
        # the Dir record is rewritten only after the check read it
        self.assertLess(rec, INSTALL.index('Echo >ENVARC:up-term/Dir "$updest"'))

    def test_uninstall_empties_but_keeps_a_drawer_that_was_there(self):
        keep = UNINSTALL.index('If EXISTS ENVARC:up-term/DirWasThere')
        self.assertEqual(UNINSTALL.count(EMPTY), 1)
        self.assertEqual(UNINSTALL.count(WHOLE), 1)
        self.assertLess(keep, UNINSTALL.index(EMPTY))
        self.assertLess(UNINSTALL.index(EMPTY), UNINSTALL.index(WHOLE))
        # the record is still there when read: ENVARC:up-term goes whole later
        self.assertLess(keep, UNINSTALL.index('Delete >NIL: ENVARC:up-term ALL QUIET'))


if __name__ == '__main__':
    unittest.main()
