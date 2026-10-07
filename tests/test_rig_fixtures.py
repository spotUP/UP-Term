#!/usr/bin/env python3
"""tools/rig on the host, against a fake agent (no emulator): gclick matches a
gadget id exactly ('90' is not '290': it clicked a radio button), and a rig run
puts back what it planted (ENVARC:Claude/remote from a killed run) even when it fails."""
import pathlib, struct, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import ami, install_rig as ir

TREE = b"W 1 \"Install\" 0 0 400 300\nG 290 10 10 20x10 radio\nG 90 100 100 40x20 button\n"

class FakeAmiga:
    def __init__(self, files, yunits=1):
        # yunits: pointer units per screen pixel vertically (2 on a native
        # non-interlaced screen such as the Replay's PAL Hires, 1 on RTG)
        self.files, self.clicks, self.cmds, self.yunits, self.ptr = dict(files), [], [], yunits, (0, 0)
    def req(self, code, payload=b'', timeout=150):
        if code == 0x0D: return TREE
        if code == 0x08:
            if payload[:1] == b'\x01': self.ptr = struct.unpack('>HH', payload[1:5])   # MOVE
            else: self.clicks.append(payload)
            return b''
        if code == 0x0B: return struct.pack('>HH', self.ptr[0], self.ptr[1] // self.yunits)
        if code == 0x03:
            p = payload.decode('latin-1')
            if p not in self.files: raise SystemExit('ERR: not found')
            return self.files[p]
        if code == 0x02:
            cmd = payload[2:].decode('latin-1'); self.cmds.append(cmd)
            if cmd.startswith('Delete'): self.files.pop(cmd.split('"')[1], None)
            return struct.pack('>I', 0) + (b'' if 'Assign LIST' not in cmd else b'GG   VTC:gg\n')
        return b''
    def put(self, local, remote):
        with open(local, 'rb') as f: self.files[remote] = f.read()

class Rig(unittest.TestCase):
    def setUp(self):
        self.fake = FakeAmiga({'ENVARC:Claude/remote': b'; c\n192.168.0.198 2323\n'})
        self.saved = (ami.req, ami.put, ir.run)
        ami.req, ami.put = self.fake.req, self.fake.put
        ir.run = lambda cmd, timeout=60: (struct.unpack('>I', self.fake.req(0x02, struct.pack('>H', 1) + cmd.encode('latin-1'))[:4])[0], 'GG   VTC:gg\n' if cmd == 'Assign LIST' else '')
    def tearDown(self): ami.req, ami.put, ir.run = self.saved

    def test_gclick_matches_the_gadget_id_exactly(self):
        ami.main(['gclick', 'Install', '90'])
        x, y = struct.unpack('>HH', self.fake.clicks[0][1:5])
        self.assertEqual((x, y), (120, 110), 'clicked the radio button 290, not the gadget 90')

    def test_gclick_on_a_non_interlaced_screen_doubles_y(self):
        # The Replay's PAL Hires screen: a click at UITREE's pixel y landed at
        # half the height until click_px scaled it (2026-10-07)
        self.fake.yunits = 2
        ami.main(['gclick', 'Install', '90'])
        x, y = struct.unpack('>HH', self.fake.clicks[0][1:5])
        self.assertEqual((x, y), (120, 220))

    def test_a_failing_run_puts_back_the_users_remote_file(self):
        fx = ir.Fixtures(); fx.take()
        self.fake.files['ENVARC:Claude/remote'] = b'127.0.0.1 2399\n'   # the rig fixture
        self.fake.files['ENV:Claude/remote'] = b'127.0.0.1 2399\n'
        try:
            try:
                raise RuntimeError('the run failed')
            finally:
                fx.restore()
        except RuntimeError: pass
        self.assertEqual(self.fake.files['ENVARC:Claude/remote'], b'; c\n192.168.0.198 2323\n')
        self.assertNotIn('ENV:Claude/remote', self.fake.files, 'absent before: absent after')

    def test_main_restores_when_the_body_raises(self):
        orig = ir._main
        def boom(dest=None):
            self.fake.files['ENVARC:Claude/remote'] = b'127.0.0.1 2399\n'
            raise SystemExit('killed')
        ir._main = boom
        try:
            with self.assertRaises(SystemExit): ir.main()
        finally: ir._main = orig
        self.assertEqual(self.fake.files['ENVARC:Claude/remote'], b'; c\n192.168.0.198 2323\n')

if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
