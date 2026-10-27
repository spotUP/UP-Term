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
        saved_t, saved_run = irg.time, irg.ir.run
        irg.time = clock

        def run(cmd, timeout=60):
            raise ConnectionRefusedError(61, 'Connection refused')
        irg.ir.run = run
        try:
            finished, log = irg.drive('refused-forever', 'Installer', None, set(), timeout=100)
        finally:
            irg.time, irg.ir.run = saved_t, saved_run
        self.assertFalse(finished)
        self.assertLess(clock.now, 400)


if __name__ == "__main__":
    unittest.main()
