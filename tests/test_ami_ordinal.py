#!/usr/bin/env python3
"""ami ordinal gadget addresses. MUI gadgets all have id 0 in the uitree, so `ui settext`/`ui click`/`gclick` could
only reach the first String gadget. 'string:N' names the N-th gadget of that kind in the window (1-based). The wiring
check drives ami.req(0x0E) (the entry `ami.py ui ...` goes through) against a fake connection and asserts which gadget
the mouse frame lands on. Host-only."""
import contextlib, os, pathlib, struct, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
os.environ.setdefault('UPTERM_RIG', '7')
import ami

GADGETS = [
    dict(id=0, kind='cycle', x=10, y=10, width=40, height=10, label='', value='', disabled=False, selected=False),
    dict(id=0, kind='string', x=10, y=30, width=80, height=10, label='', value='a', disabled=False, selected=False),
    dict(id=0, kind='row', x=0, y=0, width=0, height=0, label='r', value='', disabled=False, selected=False),
    dict(id=0, kind='string', x=10, y=50, width=80, height=10, label='', value='b', disabled=False, selected=False),
]


class FakeM:
    SYSTEM_KINDS = ('close',)
    def read_ui(self, c, every_screen=False): return [dict(windows=[dict(title='Edit', gadgets=GADGETS)])]
    def ui_windows(self, screens, title=None):
        return [(s, w) for s in screens for w in s['windows'] if title is None or title in w['title']]
    def key_combo_events(self, s): return []


class Ordinal(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(ami.ordinal_address('string:2'), ('string', 2))
        self.assertEqual(ami.ordinal_address('Cycle:1'), ('cycle', 1))
        for other in ('90', 'sys:close', 'string:0', 'string:x', 'Save', 'string:'):
            self.assertIsNone(ami.ordinal_address(other), other)

    def test_nth_of_kind_skips_other_kinds(self):
        self.assertEqual(ami.nth_of_kind(GADGETS, 'string', 2)['y'], 50)
        self.assertIsNone(ami.nth_of_kind(GADGETS, 'string', 3))

    def test_ui_click_string2_lands_on_the_second_string(self):
        sent = []
        orig = (ami._m, ami._mouse)
        ami._m = lambda: FakeM()
        ami._mouse = lambda c, action, button, count, x, y: sent.append((x, y))
        try:
            out = ami._uiact(None, b'click\tEdit\tstring:2')
        finally:
            ami._m, ami._mouse = orig
        self.assertEqual(sent, [(50, 55)])   # the sentinel: the mouse went to string #2's middle
        self.assertEqual(out, b'click at 50,55')


if __name__ == '__main__':
    unittest.main()
