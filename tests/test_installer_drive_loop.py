"""installer_rig.drive FAILs when the room page keeps sending it back to the drawer page. Before 2026-10-25 a
drawer too small for the parts chosen made drive press Proceed on drawer page, room page, drawer page, ... until
the 5400 s timeout: the drawer was typed only once, then the page was answered with Proceed every time."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

# the Installer's drawer page (askdir: 89 Make New Drawer, 92 the string) and its room message page
DRAWER = '''W 0 152 195 496x210 active "UP-Term"
G 89 160 340 238x16 custom - ""
G 92 160 360 476x14 custom - ""
G 90 281 386 238x16 custom - ""
G 91 402 386 238x16 custom - ""
'''
ROOM = '''W 0 152 195 496x210 active "UP-Term"
G 0 152 195 496x13 sys:drag - ""
G 90 281 386 238x16 custom - ""
G 91 402 386 238x16 custom - ""
'''


class Clock:
    """time for drive: sleep moves the clock, so a loop that never ends reaches the timeout at once."""
    def __init__(self):
        self.now = 0.0

    def time(self):
        return self.now

    def sleep(self, s):
        self.now += s


class RoomLoop(unittest.TestCase):
    def test_room_page_coming_back_fails_instead_of_looping(self):
        import installer_rig as irg
        clock, calls = Clock(), []

        def req(code, payload=b'', timeout=150):
            calls.append(code)
            return (DRAWER if (len(calls) // 2) % 2 == 0 else ROOM).encode('latin-1')

        saved = {k: getattr(irg, k) for k in ('time', 'installer_running', 'click', 'text_faults')}
        saved_req = irg.ami.req
        irg.time, irg.installer_running, irg.click = clock, lambda inst: True, lambda box: None
        irg.text_faults = lambda tree: ([], [])
        irg.ami.req = req
        try:
            finished, log = irg.drive('roomloop-test', 'Installer', None, set(), timeout=5400)
        finally:
            for k, v in saved.items():
                setattr(irg, k, v)
            irg.ami.req = saved_req
        self.assertFalse(finished)
        self.assertTrue(any(l.startswith('room page repeats') for l in log), log)
        self.assertLess(clock.now, 600, 'drive went on to the timeout')


class WasabidRefuses(unittest.TestCase):
    def test_refused_connections_are_busy_not_a_crash(self):
        """2026-10-27 rig 9: wasabid refused connections during the nested AmiSSL Install and the driver died with
        ConnectionRefusedError in installer_running / the UITREE ask."""
        import installer_rig as irg
        clock, asks, runs = Clock(), [], []

        def req(code, payload=b'', timeout=150):
            asks.append(code)
            if len(asks) <= 3:
                raise ConnectionRefusedError(61, 'Connection refused')
            return b'W 0 152 195 496x210 active "UP-Term"\n'

        def run(cmd, timeout=60):
            runs.append(cmd)
            if len(runs) <= 2:
                raise ConnectionRefusedError(61, 'Connection refused')
            return (0 if len(runs) < 6 else 1), ''

        saved_t, saved_req, saved_run = irg.time, irg.ami.req, irg.ir.run
        irg.time, irg.ami.req, irg.ir.run = clock, req, run
        try:
            finished, log = irg.drive('refused-test', 'Installer', None, set(), timeout=5400, poll=1)
        finally:
            irg.time, irg.ami.req, irg.ir.run = saved_t, saved_req, saved_run
        self.assertTrue(finished, log)
        self.assertGreaterEqual(len(runs), 6)
        self.assertGreaterEqual(len(asks), 4)   # three refused asks were retried

    def test_refusing_forever_still_ends_at_the_timeout(self):
        import installer_rig as irg
        clock = Clock()
        saved_t, saved_run, saved_req = irg.time, irg.ir.run, irg.ami.req
        irg.time = clock

        def run(cmd, timeout=60):
            raise ConnectionRefusedError(61, 'Connection refused')

        def req(code, payload=b'', timeout=150):
            raise ConnectionRefusedError(61, 'Connection refused')
        irg.ir.run, irg.ami.req = run, req
        try:
            finished, log = irg.drive('refused-forever', 'Installer', None, set(), timeout=100)
        finally:
            irg.time, irg.ir.run, irg.ami.req = saved_t, saved_run, saved_req
        self.assertFalse(finished)
        self.assertLess(clock.now, 400)


class ClickSpeed(unittest.TestCase):
    """2026-10-27: drive slept a fixed 3 s after every Proceed click; the next click now follows the page change."""
    PAGES = 5

    def run_install(self):
        import installer_rig as irg
        clock, state, clicks = Clock(), {'page': 0}, []

        def page(i):
            return ('W 0 152 195 496x210 active "Page %d"\nG 0 152 195 496x13 sys:drag - ""\n'
                    'G 90 281 386 238x16 custom - ""\nG 91 402 386 238x16 custom - ""\n') % i

        def req(code, payload=b'', timeout=150):
            return page(state['page']).encode('latin-1')

        def click(box):
            clicks.append(clock.now)
            state['page'] += 1   # the Installer shows the next page on its own time (here: at the click)

        saved = {k: getattr(irg, k) for k in ('time', 'installer_running', 'click', 'text_faults')}
        saved_req = irg.ami.req
        irg.time, irg.click = clock, click
        irg.installer_running = lambda inst: state['page'] < self.PAGES
        irg.text_faults = lambda tree: ([], [])
        irg.ami.req = req
        try:
            finished, log = irg.drive('clickspeed-test', 'Installer', None, set(), timeout=5400)
        finally:
            for k, v in saved.items():
                setattr(irg, k, v)
            irg.ami.req = saved_req
        return finished, log, clock, clicks

    def test_next_click_follows_the_page_change(self):
        finished, log, clock, clicks = self.run_install()
        self.assertTrue(finished, log)
        self.assertEqual(len(clicks), self.PAGES)
        gaps = [b - a for a, b in zip(clicks, clicks[1:])]
        self.assertTrue(all(g <= 0.5 for g in gaps), gaps)   # the old loop slept 3 s between clicks

    def test_total_sleep_of_five_pages_is_well_under_the_old_total(self):
        finished, log, clock, clicks = self.run_install()
        self.assertTrue(finished, log)
        old_total = 3.0 * self.PAGES   # old: time.sleep(3) after each Proceed click
        self.assertLess(clock.now, old_total / 3, clock.now)


class LinkDropped(unittest.TestCase):
    """2026-10-28 rig 9: the Amiga crashed on the Installer's last page; wasabi's "connection closed by the Amiga"
    is a SystemExit, and drive ended the run without printing the pages it had answered."""
    def test_pages_answered_are_printed_before_the_run_ends(self):
        import contextlib, io
        import installer_rig as irg
        clock, state = Clock(), {'n': 0}

        def req(code, payload=b'', timeout=150):
            state['n'] += 1
            if state['n'] > 1:
                raise SystemExit('ERR: connection closed by the Amiga')
            return ('W 0 152 195 496x210 active "Last Page"\nG 0 152 195 496x13 sys:drag - ""\n'
                    'G 90 281 386 238x16 custom - ""\nG 91 402 386 238x16 custom - ""\n').encode('latin-1')

        saved = {k: getattr(irg, k) for k in ('time', 'installer_running', 'click', 'text_faults')}
        saved_req = irg.ami.req
        irg.time, irg.click = clock, lambda box: None
        irg.installer_running = lambda inst: True
        irg.text_faults = lambda tree: ([], [])
        irg.ami.req = req
        out = io.StringIO()
        try:
            with contextlib.redirect_stdout(out), self.assertRaises(SystemExit):
                irg.drive('dropped-test', 'Installer', None, set(), timeout=5400)
        finally:
            for k, v in saved.items():
                setattr(irg, k, v)
            irg.ami.req = saved_req
        self.assertIn('the link dropped after', out.getvalue())
        self.assertIn('page [Last Page]', out.getvalue())


class NestedFinishPage(unittest.TestCase):
    """2026-10-28 rig 9: the nested Install-AmiSSL's last page ("Please reboot for modifications to take effect",
    Proceed only) was taken for the outer Installer's Reboot question; drive stopped there with "reboot page
    without a Later gadget". Only the Installer the case started asks Reboot now?"""
    def run_pages(self, pages):
        import installer_rig as irg
        clock, state, clicks = Clock(), {'page': 0}, []

        def page(i):
            if i >= len(pages):
                return ''
            title, ids, _ = pages[i]
            return ('W 0 152 195 496x210 active "%s"\nG 0 152 195 496x13 sys:drag - ""\n' % title
                    + ''.join('G %d %d 386 238x16 custom - ""\n' % (g, 281 + 121 * n) for n, g in enumerate(ids)))

        def req(code, payload=b'', timeout=150):
            return page(state['page']).encode('latin-1')

        def click(box):
            clicks.append(pages[state['page']][0])
            state['page'] += 1

        saved = {k: getattr(irg, k) for k in ('time', 'installer_running', 'click', 'text_faults', 'window_ocr')}
        saved_req = irg.ami.req
        irg.time, irg.click = clock, click
        irg.installer_running = lambda inst: state['page'] < len(pages)
        irg.text_faults = lambda tree: ([], [])
        irg.window_ocr = lambda tree, name: pages[min(state['page'], len(pages) - 1)][2]
        irg.ami.req = req
        try:
            finished, log = irg.drive('nested-end-test', 'Installer', None, set(), timeout=5400)
        finally:
            for k, v in saved.items():
                setattr(irg, k, v)
            irg.ami.req = saved_req
        return finished, log, clicks

    def test_nested_installers_last_page_is_answered_with_proceed(self):
        finished, log, clicks = self.run_pages([
            ('VTC:distkit/Install (40% done)', (90, 91), 'Install AmiSSL now'),
            ('Install-AmiSSL (100% done)', (90,), 'AmiSSL 5.27 installation finished. Please reboot for modifications'),
        ])
        self.assertTrue(finished, log)
        self.assertEqual(clicks, ['VTC:distkit/Install (40% done)', 'Install-AmiSSL (100% done)'])

    def test_outer_reboot_question_without_later_is_still_never_clicked(self):
        finished, log, clicks = self.run_pages([
            ('VTC:distkit/Install (40% done)', (90, 91), 'Install AmiSSL now'),
            ('Install-AmiSSL (100% done)', (90,), 'Please reboot for modifications'),
            ('VTC:distkit/Install (100% done)', (90,), 'Reboot now?'),
        ])
        self.assertFalse(finished, log)
        self.assertNotIn('VTC:distkit/Install (100% done)', clicks)
        self.assertTrue(any('without a Later gadget' in l for l in log), log)


if __name__ == "__main__":
    unittest.main()
