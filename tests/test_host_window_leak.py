"""tabstate.open_host / vshwin.close_vsh must not leak windows (menu rig r4, rig 7, 2026-10-27: nine "menuhost"
windows, because open_host opened a new one per row and close_vsh typed `exit` into a window it had not made
active)."""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
sys.modules.setdefault("paths", mock.MagicMock())
sys.modules.setdefault("condev_rig", mock.MagicMock())
sys.modules.setdefault("wasabi", mock.MagicMock())
import ami, tabstate, vshwin


class Desktop:
    """Fake rig: titled windows; a click activates the window only when `clicks_work`; `exit` closes the active one."""
    def __init__(self, clicks_work=True):
        self.wins, self.clicks_work, self.typed = [], clicks_work, []

    def open(self, title, **k):
        for w in self.wins:
            w['active'] = False
        self.wins.append({'title': title, 'box': (0, 0, 100, 100), 'active': False})

    def window(self, title):
        return next((w for w in self.wins if w['title'] == title), None)

    def click(self, x, y):
        if self.clicks_work:
            for w in self.wins:
                w['active'] = w is self.wins[0]

    def typeline(self, s, wait):
        act = next((w for w in self.wins if w['active']), None)
        self.typed.append((s, act and act['title']))
        if s == 'exit' and act:
            self.wins.remove(act)

    def patches(self):
        return [mock.patch.object(ami, "window", self.window), mock.patch.object(ami, "click_px", self.click),
                mock.patch.object(vshwin, "open_vsh", self.open), mock.patch.object(vshwin, "typeline", self.typeline),
                mock.patch.object(time_mod, "sleep", lambda s: None)]


import time as time_mod


class Leak(unittest.TestCase):
    def run_with(self, d, fn):
        ps = d.patches()
        for p in ps:
            p.start()
        try:
            return fn()
        finally:
            for p in ps:
                p.stop()

    def test_second_open_host_leaves_one_window(self):
        d = Desktop()
        def go():
            tabstate.open_host("menuhost")
            d.wins[0]['active'] = False       # another window took focus between rows
            tabstate.open_host("menuhost")
        self.run_with(d, go)
        self.assertEqual([w['title'] for w in d.wins], ["menuhost"])

    def test_close_never_types_into_unconfirmed_window(self):
        d = Desktop(clicks_work=False)
        d.open("menuhost"); d.open("other"); d.wins[1]['active'] = True
        ok = self.run_with(d, lambda: vshwin.close_vsh("menuhost", secs=0))
        self.assertFalse(ok)
        self.assertEqual(d.typed, [])

    def test_stuck_leftover_fails_the_row_by_name(self):
        d = Desktop(clicks_work=False)
        d.open("menuhost")
        with self.assertRaises(SystemExit) as e:
            self.run_with(d, lambda: tabstate.open_host("menuhost"))
        self.assertIn("menuhost", str(e.exception))
        self.assertEqual(len(d.wins), 1)


if __name__ == "__main__":
    unittest.main()
