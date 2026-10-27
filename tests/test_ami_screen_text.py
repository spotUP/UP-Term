#!/usr/bin/env python3
"""ami.screen_text on the host with a fake transport: it must reach typewin.screen_text, which asks the focused
UP-Term window for its rows (`VTC:UPTerm ACTIVE screen FILE`) and presses no key, touches no clipboard and clicks
nowhere; only when no UP-Term window answers does it use the old path (Select all, Copy, read the clipboard file
through snipprobe, a clearing click) in the ACTIVE window (the menu's own, titled Amarchy)."""
import pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import ami, typewin


class ScreenText(unittest.TestCase):
    def setUp(self):
        self.runs, self.keys = [], []
        self.saved = (typewin.c.run, ami.key, typewin.time.sleep, ami.window, typewin.click_in, ami.req)
        clip = {'text': '-'}
        self.dump = {'rc': 0, 'text': 'Filter: th\nTheme\n'}

        def run(cmd, *a, **k):
            self.runs.append(cmd)
            if cmd.startswith('VTC:UPTerm ACTIVE screen FILE '):
                return self.dump['rc'], ''
            if cmd.startswith('Type RAM:screen.txt') and self.dump['rc'] == 0:
                return 0, self.dump['text']
            if cmd.startswith('VTC:snipprobe CLEAR'):
                clip['text'] = '-'
            if cmd.startswith('Type '):
                return 0, clip['text']
            return 0, ''
        typewin.c.run = run
        # the fake Copy puts the menu's text on the clipboard
        ami.key = lambda code, qual=0: (self.keys.append((code, qual)),
                                        clip.update(text='Filter: th\nTheme') if code == 0x33 else None)
        typewin.time.sleep = lambda s: None
        ami.window = lambda t: None
        tree = 'S 0 640x256 depth=4 "Workbench Screen"\nW 0 0 0 640x256 active "Amarchy"\nW 1 0 0 640x256 - "UP-Term"\n'
        ami.req = lambda code, payload=b'', timeout=150: tree.encode() if code == 0x0D else b''
        typewin.click_in = lambda t: self.runs.append('click ' + t)

    def tearDown(self):
        typewin.c.run, ami.key, typewin.time.sleep, ami.window, typewin.click_in, ami.req = self.saved
        typewin._last_title = None

    def test_asks_the_window_and_touches_nothing_else(self):
        t = ami.screen_text()
        self.assertEqual(t, 'Filter: th\nTheme\n')
        self.assertIn('VTC:UPTerm ACTIVE screen FILE RAM:screen.txt', self.runs)
        self.assertEqual(self.keys, [])                                  # no Select all, no Copy
        self.assertFalse([r for r in self.runs if 'snipprobe' in r or r.startswith('click')])

    def test_no_terminal_window_falls_back_to_the_clipboard(self):
        self.dump['rc'] = 20
        t = ami.screen_text()
        self.assertIn('Filter: th', t)
        self.assertEqual(self.keys[:2], [(0x20, typewin.RAMIGA), (0x33, typewin.RAMIGA)])
        self.assertIn('VTC:snipprobe CLEAR', self.runs)
        self.assertIn('click Amarchy', self.runs)


if __name__ == '__main__':
    unittest.main()
