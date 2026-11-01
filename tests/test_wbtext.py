#!/usr/bin/env python3
"""tools/rig/wbtext.py finds the error line an icon-started UP-Term window shows (A1200, 2026-10-18: AmigaDOS 118)."""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools', 'rig'))
import wbtext

BANNER = "   R O Y A L   K I N G S   O F   T H E   A M I G A   S C E N E\nSYS: %\n"


class WbText(unittest.TestCase):
    def test_clean_window(self):
        self.assertEqual(wbtext.error_lines(BANNER), [])

    def test_error_118(self):
        t = "vshrc: line 3: Rushhours:Foo: wrong number of arguments\n" + BANNER
        self.assertEqual(wbtext.error_lines(t), ["vshrc: line 3: Rushhours:Foo: wrong number of arguments"])

    def test_other_errors(self):
        for l in ("Unknown command Foo", "Can't open ENVARC:vsh/vshrc", "Foo: object not found", "Failed returncode 20"):
            self.assertEqual(wbtext.error_lines(l + "\n"), [l])


if __name__ == '__main__':
    unittest.main()
