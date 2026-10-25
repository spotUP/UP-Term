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


if __name__ == "__main__":
    unittest.main()
