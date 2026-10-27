#!/usr/bin/env python3
"""ami.py open starts an icon the way a Workbench double-click does. `Run UP-Term` printed [CLI n] and opened nothing:
SYS:System/UP-Term is a text note whose .info carries the default tool and WINDOW tool type, so the line must go
through wbrun, detached (Run >NIL:) because wbrun waits for the tool. Host-only, no connection made."""
import os, pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
os.environ.setdefault('UPTERM_RIG', '7')
import ami


class AmiOpen(unittest.TestCase):
    def test_line_goes_through_wbrun_detached(self):
        self.assertEqual(ami.open_command('SYS:System/UP-Term'), 'Run >NIL: RAM:wbrun SYS:System/UP-Term')

    def test_not_a_plain_run_of_the_icon(self):
        self.assertNotIn('Run >NIL: SYS:System/UP-Term', ami.open_command('SYS:System/UP-Term'))

    def test_wbrun_is_the_build_product(self):
        self.assertTrue(ami.WBRUN_LOCAL.endswith('build/amiga/wbrun'))


if __name__ == '__main__':
    unittest.main()
