#!/usr/bin/env python3
"""vshwin.close_vsh clicked the centre of its window to make it active. After a cold boot the boot UP-Term window
covered menuhost, the click went to UP-Term, and close_vsh refused to type exit (s12, ledger TMUX-MENU-SWFAIL).
ami.raise_window brings the window to the front when another one covers the click point, with wasabi's own
covered() and ui_front(); close_vsh and typewin.click_in call it before they click. Host-only: the connection and
the UI list are fakes, covered() is wasabi's real one."""
import contextlib, os, pathlib, sys, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
os.environ.setdefault('UPTERM_RIG', '7')
import ami
import vshwin


def win(title, left, top, width, height, active=False):
    return {'addr': '%x' % id(title), 'title': title, 'left': left, 'top': top, 'width': width, 'height': height,
            'active': active, 'gadgets': [], 'menus': []}


# grab space, rows doubled (row_scale 2); windows front first, as wasabi's read_ui gives them
UPTERM = win('UP-Term', 0, 0, 640, 400, active=True)
MENUHOST = win('menuhost', 0, 40, 600, 400)
SCREENS = [{'title': 'Workbench Screen', 'row_scale': 2, 'front': True, 'windows': [UPTERM, MENUHOST]}]


class RaiseWindow(unittest.TestCase):
    def setUp(self):
        self.m = ami._m()
        self.fronted = []
        p = [mock.patch.object(ami, '_conn', lambda *a, **k: contextlib.nullcontext(object())),
             mock.patch.object(self.m, 'read_ui', lambda c, every_screen=False: SCREENS),
             mock.patch.object(self.m, 'ui_front', lambda c, w: self.fronted.append(w['title']))]
        for x in p:
            x.start()
            self.addCleanup(x.stop)

    def test_covered_window_comes_to_front(self):
        self.assertTrue(ami.raise_window('menuhost'))
        self.assertEqual(self.fronted, ['menuhost'])

    def test_point_in_screen_pixels_is_scaled_to_grab_space(self):
        # screen row 210 = grab row 420: below UP-Term (rows 0..399), so nothing covers it
        self.assertTrue(ami.raise_window('menuhost', 300, 210))
        self.assertEqual(self.fronted, [])

    def test_front_window_is_left_alone(self):
        self.assertTrue(ami.raise_window('UP-Term'))
        self.assertEqual(self.fronted, [])

    def test_missing_window(self):
        self.assertFalse(ami.raise_window('nothere'))


class CloseVshRaisesFirst(unittest.TestCase):
    def test_raise_before_click(self):
        calls = []
        gone = iter([{'box': (0, 20, 600, 200), 'active': False}, None])
        with mock.patch.object(ami, 'window', lambda t: next(gone)), \
                mock.patch.object(ami, 'raise_window', lambda t, x, y: calls.append(('raise', t, x, y))), \
                mock.patch.object(ami, 'click_px', lambda x, y: calls.append(('click', x, y))), \
                mock.patch.object(vshwin.time, 'sleep', lambda s: None):
            self.assertTrue(vshwin.close_vsh('menuhost'))
        self.assertEqual(calls, [('raise', 'menuhost', 300, 120), ('click', 300, 120)])


if __name__ == '__main__':
    unittest.main()
