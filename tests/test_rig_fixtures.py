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

    def test_installer_rig_clicks_proceed_on_a_non_interlaced_screen(self):
        # 3.2's PAL Workbench: installer_rig clicked Proceed (y 191) at y 95, in the page's text,
        # and the Installer's first page never moved (2026-10-08)
        import installer_rig
        self.fake.yunits = 2
        installer_rig.click((100, 100, 40, 20))
        x, y = struct.unpack('>HH', self.fake.clicks[0][1:5])
        self.assertEqual((x, y), (120, 220))

    def test_a_shot_with_indices_past_the_palette_keeps_its_rows(self):
        # 3.2's native 16-colour screen: SHOT sent indices 16 and 32; a palette lookup past the end gave
        # no bytes and sheared every later row (2026-10-08). Only the screen's depth counts.
        pal = bytes([170, 170, 170, 0, 0, 0]) + bytes(14 * 3)
        px = bytes([0, 1, 16, 33, 1, 0, 32, 17])        # 4x2: 16 -> 0, 33 -> 1, 32 -> 0, 17 -> 1
        body = bytes([1, 0]) + struct.pack('>HHH', 4, 2, 16) + pal + px
        saved = ami.req
        ami.req = lambda code, payload=b'', timeout=150: body
        try:
            w, h, rows = ami.grab()
        finally:
            ami.req = saved
        g, k = bytes([170, 170, 170]), bytes(3)
        self.assertEqual(rows, [g + k + g + k, k + g + g + k])

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

class Fingerprints(unittest.TestCase):
    """K5: a rig verdict's fingerprint covers the scripts as the kit build assembles them,
    so a change in a dist/common/ part (not in the dist/ templates) invalidates old verdicts."""
    def test_a_change_in_dist_common_changes_the_fingerprint(self):
        import shutil, tempfile
        sys.path.insert(0, str(ROOT / 'tools'))
        import assign_rig, installer_rig, mkinstaller
        before = (assign_rig.fingerprint(), installer_rig.fingerprint())
        tmp = pathlib.Path(tempfile.mkdtemp())
        for d in ('common', 'values'):
            shutil.copytree(ROOT / 'dist' / d, tmp / 'dist' / d)
        part = tmp / 'dist/common/record.dos'
        part.write_text(part.read_text(encoding='latin-1') + '; changed\n', encoding='latin-1')
        saved = mkinstaller.ROOT
        mkinstaller.ROOT = tmp
        try:
            after = (assign_rig.fingerprint(), installer_rig.fingerprint())
        finally:
            mkinstaller.ROOT = saved
            shutil.rmtree(tmp)
        self.assertNotEqual(after[0], before[0], 'assign_rig fingerprint ignores dist/common')
        self.assertNotEqual(after[1], before[1], 'installer_rig fingerprint ignores dist/common')
        self.assertEqual((assign_rig.fingerprint(), installer_rig.fingerprint()), before)

class Step12(unittest.TestCase):
    """M9: the fixture answers every route and answer text of amigapi_rig.py's step 12 (host, no rig)."""
    BODY = {'anthropic': {'messages': [{'role': 'user', 'content': 'hello'}]},
            'completions': {'messages': [{'role': 'user', 'content': 'hello'}]},
            'responses': {'input': [{'role': 'user', 'content': 'hello'}]},
            'azure': {'input': [{'role': 'user', 'content': 'hello'}]},
            'google': {'contents': [{'parts': [{'text': 'hello'}]}]},
            'bedrock': {'messages': [{'content': [{'text': 'hello'}]}]},
            'codex': {'input': [{'role': 'user', 'content': 'hello'}]}}

    def test_the_fixture_serves_each_adapter_route(self):
        import json, subprocess, time, urllib.request
        import amigapi_rig as ar
        port = 18312
        fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools/claude_fixture.py'), '--port', str(port), '--delay', '0'],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            time.sleep(1)
            self.assertEqual(len({r[1] for r in ar.ADAPTERS_12}), 7)
            for tag, _, _, _, route, _ in ar.ADAPTERS_12:
                req = urllib.request.Request('http://127.0.0.1:%d%s' % (port, route), json.dumps(self.BODY[tag]).encode(),
                                             {'originator': 'pi'}, method='POST')
                self.assertEqual(urllib.request.urlopen(req, timeout=10).status, 200, route)
        finally:
            fx.terminate()
            out = fx.communicate()[0]
        for _, _, _, _, route, _ in ar.ADAPTERS_12:
            self.assertEqual(sum(1 for l in out.splitlines() if l.startswith('[INFO] ' + route + ' ')), 1, route)
        self.assertEqual(sum(1 for l in out.splitlines() if l == '[INFO]   codex originator=pi'), 1)    # the rig step reads this line

if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
