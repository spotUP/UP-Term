#!/usr/bin/env python3
"""tools/rig on the host, against a fake agent (no emulator): gclick matches a
gadget id exactly ('90' is not '290': it clicked a radio button), and a rig run
puts back what it planted (ENVARC:Claude/remote from a killed run) even when it fails."""
import os, pathlib, shlex, struct, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import ami, install_rig as ir

TREE = b"W 1 \"Install\" 0 0 400 300\nG 290 10 10 20x10 radio\nG 90 100 100 40x20 button\n"

class FakeAmiga:
    def __init__(self, files, yunits=1):
        # yunits: wasabi's mouse units per uitree row (the screen's row_scale: 2 on a native non-interlaced
        # screen such as the Replay's PAL Hires, 1 on RTG); ami.pointer_scale() is the one place it is read
        self.files, self.clicks, self.cmds, self.yunits = dict(files), [], [], yunits
    def pointer_scale(self): return 1.0, float(self.yunits)
    def req(self, code, payload=b'', timeout=150):
        if code == 0x0D: return TREE
        if code == 0x08:
            self.clicks.append(payload)
            return b''
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
        self.saved = (ami.req, ami.put, ami.pointer_scale, ir.run)
        ami.req, ami.put, ami.pointer_scale = self.fake.req, self.fake.put, self.fake.pointer_scale
        ir.run = lambda cmd, timeout=60: (struct.unpack('>I', self.fake.req(0x02, struct.pack('>H', 1) + cmd.encode('latin-1'))[:4])[0], 'GG   VTC:gg\n' if cmd == 'Assign LIST' else '')
    def tearDown(self): ami.req, ami.put, ami.pointer_scale, ir.run = self.saved

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

    def test_the_migrate_case_deletes_the_old_drawer_with_its_icon(self):
        # VTCX:Apps/UP-Term.info stayed on rig 1 after the drawer went: a drawer icon nothing opens (2026-10-14)
        import installer_rig
        cmd = installer_rig.drawer_delete("VTC:KitTest/UP-Term")
        self.assertIn('"VTC:KitTest/UP-Term"', cmd)
        self.assertIn('"VTC:KitTest/UP-Term.info"', cmd)
        self.assertIn(' ALL', cmd)

    def test_a_case_removes_the_parent_drawer_it_made_with_its_icon(self):
        # VTCX:Apps.info stayed on rig 1 after Apps was deleted (2026-10-14): the Installer writes the parent's .info
        import installer_rig
        made = installer_rig.dest_cleanup("VTC:KitTest/UP-Term", False)
        self.assertEqual(made, [installer_rig.drawer_delete("VTC:KitTest/UP-Term"), installer_rig.drawer_delete("VTC:KitTest")])
        self.assertIn('"VTC:KitTest.info"', made[1])
        # a parent that was there before is not ours; nor is a volume root
        self.assertEqual(len(installer_rig.dest_cleanup("VTC:KitTest/UP-Term", True)), 1)
        self.assertEqual(len(installer_rig.dest_cleanup("VTC:UP-Term", False)), 1)

    def test_a_shot_keeps_its_rows(self):
        # Restated for wasabi: its grab is RGB, there is no palette to index past (the 2026-10-08 palette shear cannot
        # happen). What is left to guard is that grab() cuts the reply into h rows of w*3 bytes, none sheared.
        g, k = bytes([170, 170, 170]), bytes(3)
        px = g + k + g + k + k + g + g + k       # 4x2 RGB
        body = bytes([2, 0]) + struct.pack('>HHH', 4, 2, 0) + px
        saved = ami.req
        ami.req = lambda code, payload=b'', timeout=150: body
        try:
            w, h, rows = ami.grab()
        finally:
            ami.req = saved
        self.assertEqual((w, h), (4, 2))
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

    def test_restore_keeps_a_mixed_case_assign_that_was_there_before(self):
        # FIXTURE_ASSIGNS spells UP-Term in mixed case, take() keys the saved assigns in upper case:
        # the lookup missed and restore() removed the rig's own UP-Term: (2026-10-19)
        listing = 'GG   VTC:gg\nUP-Term   Rushhours:UpkgRig/UP-Term\n'
        def run(cmd, timeout=60):
            rc = struct.unpack('>I', self.fake.req(0x02, struct.pack('>H', 1) + cmd.encode('latin-1'))[:4])[0]
            return rc, listing if cmd == 'Assign LIST' else ''
        ir.run = run
        fx = ir.Fixtures(); fx.take(); fx.restore()
        self.assertIn('Assign UP-Term: "Rushhours:UpkgRig/UP-Term"', self.fake.cmds)
        self.assertNotIn('Assign >NIL: UP-Term:', self.fake.cmds)

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

class InstallCases(unittest.TestCase):
    """K2-10: install_rig.py's case table, its verdicts and the scripts it writes, on the host (no rig)."""
    def setUp(self):
        sys.path.insert(0, str(ROOT / 'tools'))
        import pkgs, verdicts
        self.pkgs, self.verdicts = pkgs, verdicts
        self.addons = [r['id'] for r in pkgs.lha_rows() if r['id'] != 'base']
        import tempfile
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.env = {k: os.environ.get(k) for k in ('UPTERM_KITS', 'UPTERM_KIT')}

    def tearDown(self):
        for k, v in self.env.items():
            if v is None: os.environ.pop(k, None)
            else: os.environ[k] = v

    def kit(self, pkg, manifest=b'm\n'):
        d = self.tmp / ir.archive(pkg) / 'Files'
        d.mkdir(parents=True, exist_ok=True)
        (d / 'MANIFEST').write_bytes(manifest)
        os.environ['UPTERM_KITS'] = str(self.tmp)

    def test_the_table_names_every_add_on_of_packages_txt(self):
        names = list(ir.case_table())
        for pkg in self.addons:
            self.assertIn(pkg, names, 'no case for the package %s' % pkg)
        for case in ('base', 'base-dest', 'base-move', 'base-libs', 'base-var', 'all', 'legacy', 'nobase'):
            self.assertIn(case, names)
        self.assertEqual(len(names), len(set(names)))

    def test_every_case_installs_the_base_it_needs(self):
        bare = {p + '-bare' for p in ir.standalone_ids()} | {'python-ixemul'}
        for name, (pk, _, fn) in ir.case_table().items():
            if name in bare:
                self.assertNotIn('base', pk, name + ' runs on a Workbench with no UP-Term')
            else:
                self.assertIn('base' if name != 'nobase' else ir.based_ids()[0], pk, name)
            self.assertTrue(callable(fn), name)
        self.assertEqual(ir.standalone_ids(), ['claude', 'pi', 'python'])
        self.assertEqual(ir.based_ids(), ['nvim'])

    def test_a_pass_with_the_same_kit_is_skipped_and_a_changed_archive_runs_again(self):
        for pkg in ir.package_ids(): self.kit(pkg)
        table = ir.case_table()
        store = self.verdicts.Store(self.tmp / 'install')
        fp = lambda c: ir.case_fingerprint(table[c])
        store.record('nvim', True, 'ok', fp('nvim'))
        run, skip = self.verdicts.select(store, list(table), fp)
        self.assertIn('nvim', [c for c, _ in skip])
        self.kit('nvim', b'a different payload\n')
        run, skip = self.verdicts.select(store, list(table), fp)
        self.assertIn('nvim', [c for c, _ in run], 'a changed nvim archive must run its case again')
        self.assertIn('all', [c for c, _ in run])

    def test_a_case_does_not_follow_an_archive_it_does_not_install(self):
        for pkg in ir.package_ids(): self.kit(pkg)
        table = ir.case_table()
        before = ir.case_fingerprint(table['pi'])
        self.kit('nvim', b'changed\n')
        self.assertEqual(ir.case_fingerprint(table['pi']), before, 'pi installs the base and pi only')
        self.assertNotEqual(ir.case_fingerprint(table['nobase']), ir.case_fingerprint(table['base']))

    def test_the_legacy_case_follows_the_legacy_kit(self):
        for pkg in ir.package_ids(): self.kit(pkg)
        saved = ir.LEGACY_KIT
        try:
            ir.LEGACY_KIT = self.tmp / 'legacy'
            (ir.LEGACY_KIT / 'Files').mkdir(parents=True)
            (ir.LEGACY_KIT / 'Files/MANIFEST').write_bytes(b'one')
            a = ir.case_fingerprint(ir.case_table()['legacy'])
            (ir.LEGACY_KIT / 'Files/MANIFEST').write_bytes(b'two')
            self.assertNotEqual(a, ir.case_fingerprint(ir.case_table()['legacy']))
        finally:
            ir.LEGACY_KIT = saved

    def test_the_flags(self):
        names = list(ir.case_table())
        self.assertEqual(ir.parse_args(['--only', 'nvim', '--case', 'pi'], names)[0], ['nvim', 'pi'])
        self.assertEqual(ir.parse_args(['--failed', '--force'], names)[1:3], (True, True))
        self.assertEqual(ir.parse_args(['--default', '--dest', '--move', '--var', '--libs'], names)[0],
                         ['base', 'base-dest', 'base-move', 'base-var', 'base-libs'])
        with self.assertRaises(SystemExit): ir.parse_args(['--only'], names)
        with self.assertRaises(SystemExit): ir.parse_args(['--nonsense'], names)

    def test_the_add_on_install_lines_use_keywords_the_install_dos_has(self):
        for pkg in self.addons:
            key = ir.assembled_scripts(pkg)[1].decode('latin-1').splitlines()[0]
            self.assertTrue(key.startswith('.KEY DIR/A,STEP/K'), key)
            words = {w.split('/')[0] for w in key.split()[1].split(',')}
            line = ir.addon_install_line(pkg)
            self.assertIn('VTC:kit-%s/Files/install.dos' % pkg, line)
            for kw in shlex.split(line)[3:]:
                self.assertIn(kw.split('=')[0], words, '%s: %s is not in %s' % (pkg, kw, key))

    def test_the_base_install_lines_use_keywords_the_base_install_dos_has(self):
        key = ir.assembled_scripts('base')[1].decode('latin-1').splitlines()[0]
        words = {w.split('/')[0] for w in key.split()[1].split(',')}
        for kw in 'NOCONSOLE NODEVICE CONSOLE DEVICE SHELLICON DEST'.split():
            self.assertIn(kw, words)
        self.assertNotIn('PYTHON', words, 'the base lost Python; the rig must not pass it')
        self.assertNotIn('NVIM', words)
        self.assertNotIn('REMOTE', words)
        legacy = ir.LEGACY_KIT / 'Files/install.dos'
        if legacy.exists():
            lkey = legacy.read_text(encoding='latin-1').splitlines()[0]
            lwords = {w.split('/')[0] for w in lkey.split()[1].split(',')}
            for kw in shlex.split(ir.legacy_install_line())[3:]:
                self.assertIn(kw.split('=')[0], lwords, 'the single kit has no %s' % kw)

    def test_prepare_unpacks_each_add_on_with_its_run_scripts(self):
        calls = []
        saved = (ir.VTC, ir.KIT, ir.run, ir.SYSVOL)
        try:
            ir.VTC = self.tmp / 'vtc'; ir.VTC.mkdir()
            (self.tmp / 'basekit').mkdir(); (self.tmp / 'basekit/Install').write_text('x')
            ir.KIT = self.tmp / 'basekit'
            for pkg in ir.package_ids(): self.kit(pkg)
            (self.tmp / ir.archive('nvim') / 'Install').write_text('i')
            (ROOT / 'build/amiga').mkdir(parents=True, exist_ok=True)
            made = []
            for n in ("ptytest", "iconprobe", "wbrun", "conwho", "UPConsole"):
                f = ROOT / 'build/amiga' / n
                if not f.exists(): f.write_bytes(b''); made.append(f)
            ir.run = lambda cmd, timeout=60: calls.append(cmd) or (0, '')
            try:
                ir.prepare(None, ['nvim'])
            finally:
                for f in made: f.unlink()
            self.assertTrue((ir.VTC / 'kit-nvim/Install').exists())
            self.assertEqual((ir.VTC / 'runinstall-nvim').read_text(), ir.addon_install_line('nvim') + '\n')
            un = (ir.VTC / 'rununinstall-nvim').read_text()
            self.assertIn('Set upvardelete YES', un); self.assertIn('CD VTC:kit-nvim', un); self.assertIn('Execute Uninstall', un)
            self.assertFalse((ir.VTC / 'kit-pi').exists(), 'only the add-ons asked for are unpacked')
        finally:
            ir.VTC, ir.KIT, ir.run, ir.SYSVOL = saved

    def test_run_long_keeps_its_wrapper_alive_when_the_script_returns_20(self):
        written = {}
        saved = (ir.VTC, ir.ami.req, ir.SHOW)
        try:
            ir.VTC = self.tmp / 'vtc2'; ir.VTC.mkdir()
            ir.SHOW = ''
            def req(code, payload=b'', timeout=150):
                written['wrap'] = (ir.VTC / 'longwrap').read_text()
                (ir.VTC / 'longdone').write_text('20\n')
                return struct.pack('>I', 0) + b''
            ir.ami.req = req
            time_sleep = ir.time.sleep; ir.time.sleep = lambda s: None
            try:
                rc, _ = ir.run_long('runinstall-nvim')
            finally:
                ir.time.sleep = time_sleep
        finally:
            ir.VTC, ir.ami.req, ir.SHOW = saved
        self.assertEqual(rc, 20)
        self.assertTrue(written['wrap'].startswith('FailAt 1000\n'), 'a script ending with 20 stopped the wrapper at FailAt 10')

    def test_run_long_returns_the_agents_code_when_quit_ended_the_wrapper(self):
        """nobase waited out its 1500 s timeout: `Quit 20` in install.dos ends the wrapper too (FailAt 1000
        does not keep it alive), so longdone is never written; the agent's own return code is the answer."""
        saved = (ir.VTC, ir.ami.req, ir.SHOW)
        try:
            ir.VTC = self.tmp / 'vtc3'; ir.VTC.mkdir()
            ir.SHOW = ''
            ir.ami.req = lambda code, payload=b'', timeout=150: struct.pack('>I', 20) + b'refused\n'
            rc, out = ir.run_long('runinstall-nvim', 5)
        finally:
            ir.VTC, ir.ami.req, ir.SHOW = saved
        self.assertEqual((rc, out), (20, 'refused\n'))


class RigMachine(unittest.TestCase):
    """Every test rig is the 68060 at full speed unless --a1200 asks for the stock 020 (owner 2026-10-13)."""
    def machine(self, *flags):
        import subprocess, sys
        code = ("import sys; sys.argv = ['rig.py'] + %r; sys.path.insert(0, %r); import rig; print(rig.M060, rig.MAX)"
                % (['start'] + list(flags), str(pathlib.Path(ir.__file__).parent)))
        return subprocess.run([sys.executable, '-I', '-c', code], capture_output=True, text=True).stdout.split()

    def test_the_default_machine_is_the_060(self):
        self.assertEqual(self.machine(), ['True', 'True'])

    def test_a1200_asks_for_the_stock_020(self):
        for f in ('--a1200', '--stock', '--exact', '--fast'):
            self.assertEqual(self.machine(f), ['False', 'False'], f)


class ScriptedAmiga:
    """Just enough Amiga for the add-on cases of install_rig.py to run on the host: it keeps the files an
    Install and an Uninstall put down and answers the commands the checks send. `skip` names a thing a
    broken add-on forgets (the case must then fail)."""
    def __init__(self, pkgs, skip=()):
        self.rows = {r['id']: r for r in pkgs.rows()}
        self.skip, self.files, self.log = set(skip), set(), []
        self.startup0 = '; user line\n'
        self.startup = self.startup0
        self.base = False

    def _add(self, pkg):
        r = self.rows[pkg]
        rec = r['record']
        for f in ir.ADDON_FILES[pkg]:
            self.files.add(f)
        if 'record' not in self.skip:
            self.files |= {rec + '/MANIFEST', rec + '/VERSIONS', rec + '/Uninstall'}
        if pkg == 'claude':
            self.files |= {'ENVARC:Claude/remote', 'C:uptelnet'}
            if 'oldclaude' not in self.skip:   # install.dos deletes the 1.0 leftovers (NL4)
                self.files -= set(ir.OLD_CLAUDECODE)
        if pkg == 'python':
            self.files.add('ENVARC:upkg/ixemul/needs/Python3')
            self.startup += ';BEGIN Python3\nIf EXISTS SYS:Python3\n  Assign Python3: SYS:Python3\nEndIf\n;END Python3\n'

    def _remove(self, pkg):
        rec = self.rows[pkg]['record']
        if 'uninstall' in self.skip and pkg == 'python':
            self.files -= set(ir.ADDON_FILES[pkg]) | {f for f in self.files if f.startswith(rec)}
            return
        self.files -= set(ir.ADDON_FILES[pkg]) | {f for f in self.files if f.startswith(rec)}
        if pkg == 'claude' and not self.base:
            self.files.discard('C:uptelnet')
        if pkg == 'python':
            self.files.discard('ENVARC:upkg/ixemul/needs/Python3')
            self.startup = self.startup.replace(';BEGIN Python3\nIf EXISTS SYS:Python3\n  Assign Python3: SYS:Python3\nEndIf\n;END Python3\n', '')

    def python_block(self):
        return ';BEGIN Python3' in self.startup

    def run(self, cmd, timeout=60):
        self.log.append(cmd)
        c = cmd.split('\n')[-1]
        if c.startswith('List >NIL: '):
            p = c[11:].strip('"')
            return (0 if p in self.files or any(f.startswith(p + '/') for f in self.files) else 5), ''
        if c == 'Type S:User-Startup': return 0, self.startup
        if c == 'Type S:Startup-Sequence': return 0, 'ss'
        if c == 'Type UP-Term:UP-Term.version': return 0, '$VER: UP-Term 1.0 (13.10.2026)'
        if c.startswith('Type ') and c.endswith('/VERSIONS'):
            rec = c[5:-len('/VERSIONS')]
            r = [x for x in self.rows.values() if x['record'] == rec][0]
            return 0, '%s %s abc 2026-10-13\n' % (r['archive'], r['version'])
        if c == 'C:vsh -c "ls --version"': return 0, 'ls (GNU coreutils) 5.2.1'
        if c.endswith('C:Claude --version'): return 0, 'Claude 1.0'
        if c == 'Type ENVARC:Claude/remote': return 0, '127.0.0.1 2399'
        if c == 'C:amiga-pi --version': return 0, 'amiga-pi 1.2.0'
        if c == 'Type ENVARC:amiga-pi/settings.json': return 0, 'userfile-keepme'
        if c.startswith('Echo >ENVARC:amiga-pi'): return 0, ''
        if c.startswith('Echo >') and c.split()[1][1:].startswith(('C:ClaudeCode', 'UP-Term:ClaudeCode')):
            self.files.add(c.split()[1][1:])
            return 0, ''
        if 'python3 -c "print(6*7)"' in c: return 0, '42'
        if c == 'C:vsh VTC:py8.sh': return 0, '8'
        if c.startswith('Assign >NIL: Python3: EXISTS'): return (0 if self.python_block() else 5), ''
        if c.startswith('Assign >NIL: UP-Term: EXISTS'): return (0 if self.base else 5), ''
        if c.startswith('Search >NIL: S:User-Startup'):
            want = c.split('"')[1]
            return (0 if want in self.startup else 5), ''
        if c.startswith('GetEnv UPTCHECK'): return 5, ''
        if c.startswith('List LIBS:'): return 0, 'ixemul.library 100\nixnet.library 50\n'
        return 0, ''

    def run_long(self, script, timeout=1500):
        self.log.append(script)
        if script == 'runinstall':
            self.base = True
            self.files |= {'UP-Term:UP-Term.version', 'UP-Term:VERSIONS', 'SYS:UP-Term', 'C:uptelnet'}
            self.startup += ';BEGIN UP-Term assign\nAssign UP-Term: SYS:UP-Term\n;END UP-Term assign\n'
        elif script.startswith('runinstall-'):
            self._add(script[len('runinstall-'):])
        elif script.startswith('rununinstall-'):
            self._remove(script[len('rununinstall-'):])
        elif script == 'rununinstall':
            for pkg in ir.based_ids(): self._remove(pkg)
            self.files = {f for f in self.files if not f.startswith(('UP-Term', 'SYS:UP-Term'))}
            if 'ENVARC:upkg/up-claude/MANIFEST' not in self.files: self.files.discard('C:uptelnet')
            self.base = False
            self.startup = self.startup.replace(';BEGIN UP-Term assign\nAssign UP-Term: SYS:UP-Term\n;END UP-Term assign\n', '')
        return 0, ''

    def run_slow(self, name, cmd, timeout=600):
        self.log.append(name)
        return 0, 'NVIM v0.12.0'


class AddonCases(unittest.TestCase):
    """case_addon and case_all of install_rig.py on a scripted Amiga (no rig): the case installs the base and the
    add-on through the add-on's own run script (call count), checks its record and the program answering, and
    fails when an add-on forgets its record or leaves its block in S:User-Startup behind."""
    def play(self, case, skip=()):
        sys.path.insert(0, str(ROOT / 'tools'))
        import pkgs, verdicts
        sim = ScriptedAmiga(pkgs, skip)
        saved = (ir.run, ir.run_long, ir.run_slow, ir.prepare, ir.remote_banner, ir.time.sleep)
        ir.run, ir.run_long, ir.run_slow = sim.run, sim.run_long, sim.run_slow
        ir.prepare = lambda dest, addons=(): sim.log.append('prepare %s' % ','.join(addons))
        ir.remote_banner = lambda ck: sim.log.append('banner')
        ck = verdicts.Checks()
        try:
            case(ck)
        finally:
            ir.run, ir.run_long, ir.run_slow, ir.prepare, ir.remote_banner, ir.time.sleep = saved
        return ck, sim

    def ids(self):
        return ir.addon_ids()

    def test_every_add_on_case_drives_its_own_install_and_uninstall(self):
        for pkg in self.ids():
            ck, sim = self.play(lambda ck, pkg=pkg: ir.case_addon(ck, pkg))
            self.assertEqual(ck.failed, [], pkg)
            self.assertIn('runinstall-' + pkg, sim.log)
            self.assertIn('rununinstall-' + pkg, sim.log)
            self.assertEqual([l for l in sim.log if l.startswith('runinstall-')], ['runinstall-' + pkg], 'only its own')
            self.assertGreater(ck.total, 15, pkg)

    def test_the_all_case_removes_every_add_on_with_the_base_alone(self):
        ck, sim = self.play(ir.case_all)
        self.assertEqual(ck.failed, [])
        self.assertEqual([l for l in sim.log if l.startswith('runinstall-')], ['runinstall-' + i for i in self.ids()])
        self.assertEqual([l for l in sim.log if l.startswith('rununinstall-')],
                         ['rununinstall-python', 'rununinstall-claude', 'rununinstall-pi'],
                         'Python3 while the base is there; Claude and amiga-pi after it (stand-alone); nvim goes with the base')
        self.assertEqual(sim.files - {'ENVARC:Claude/remote'}, set(), 'the base Uninstall left files (only the user\'s ENVARC:Claude stays)')

    def test_every_stand_alone_archive_installs_and_uninstalls_with_no_base(self):
        for pkg in ir.standalone_ids():
            ck, sim = self.play(lambda ck, pkg=pkg: ir.case_bare(ck, pkg))
            self.assertEqual(ck.failed, [], pkg)
            self.assertFalse(sim.base)
            self.assertIn('runinstall-' + pkg, sim.log)
            self.assertIn('rununinstall-' + pkg, sim.log)
            self.assertNotIn('runinstall', sim.log, 'the base is not installed')
            self.assertEqual(sim.files, {'ENVARC:Claude/remote'} if pkg == 'claude' else set())

    def test_python_ixemul_case_leaves_a_library_that_is_new_enough(self):
        ck, sim = self.play(ir.case_python_ixemul)
        self.assertEqual(ck.failed, [])
        self.assertNotIn('ENVARC:upkg/ixemul/installed-by-Python3', sim.files)

    def test_an_add_on_without_its_record_fails_its_case(self):
        ck, _ = self.play(lambda ck: ir.case_addon(ck, 'nvim'), skip=('record',))
        self.assertTrue(any('its record has' in f for f in ck.failed), ck.failed)

    def test_an_install_that_leaves_the_1_0_claudecode_behind_fails_the_claude_cases(self):
        ck, sim = self.play(lambda ck: ir.case_addon(ck, 'claude'))
        self.assertEqual(ck.failed, [])
        self.assertTrue(any(l.startswith('Echo >UP-Term:ClaudeCode.info') for l in sim.log), 'the 1.0 icon was planted')
        ck, _ = self.play(lambda ck: ir.case_addon(ck, 'claude'), skip=('oldclaude',))
        self.assertTrue(any('no UP-Term:ClaudeCode.info' in f for f in ck.failed), ck.failed)
        ck, _ = self.play(lambda ck: ir.case_bare(ck, 'claude'), skip=('oldclaude',))
        self.assertTrue(any('no C:ClaudeCode' in f for f in ck.failed), ck.failed)

    def test_python_leaving_its_block_behind_fails_its_case(self):
        sim_cls = ScriptedAmiga
        orig = sim_cls._remove
        def keeps_block(self, pkg):
            keep = self.startup
            orig(self, pkg)
            if pkg == 'python': self.startup = keep
        sim_cls._remove = keeps_block
        try:
            ck, _ = self.play(lambda ck: ir.case_addon(ck, 'python'))
        finally:
            sim_cls._remove = orig
        self.assertTrue(any('block is out of S:User-Startup' in f for f in ck.failed), ck.failed)


class InstallerCases(unittest.TestCase):
    """K2-11: installer_rig.py's add-on and migrate cases on the host: the case table, the fingerprints, the
    manifest helpers migrate uses, and the add-on case on a scripted Amiga (the Installer replaced by the
    kit's install.dos, recorded call by call)."""
    def setUp(self):
        sys.path.insert(0, str(ROOT / 'tools'))
        import pkgs, verdicts, installer_rig
        self.ir2, self.pkgs, self.verdicts = installer_rig, pkgs, verdicts
        import tempfile
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.env = os.environ.get('UPTERM_KITS')

    def tearDown(self):
        if self.env is None: os.environ.pop('UPTERM_KITS', None)
        else: os.environ['UPTERM_KITS'] = self.env

    def kit(self, pkg, manifest=b'm\n'):
        d = self.tmp / ir.archive(pkg) / 'Files'
        d.mkdir(parents=True, exist_ok=True)
        (d / 'MANIFEST').write_bytes(manifest)
        os.environ['UPTERM_KITS'] = str(self.tmp)

    def test_there_is_an_installer_case_for_every_add_on_and_one_for_the_migration(self):
        for r in self.pkgs.lha_rows():
            if r['id'] != 'base':
                self.assertIn('addon-' + r['id'], self.ir2.CASES)
        self.assertIn('migrate', self.ir2.CASES)

    def test_an_add_on_case_follows_its_archive_and_the_base_only(self):
        for pkg in ir.package_ids(): self.kit(pkg)
        a, b = self.ir2.fingerprint('addon-pi'), self.ir2.fingerprint('default')
        self.kit('nvim', b'changed\n')
        self.assertEqual((self.ir2.fingerprint('addon-pi'), self.ir2.fingerprint('default')), (a, b))
        n = self.ir2.fingerprint('addon-nvim')
        self.kit('nvim', b'changed again\n')
        self.assertNotEqual(self.ir2.fingerprint('addon-nvim'), n)
        self.assertNotEqual(self.ir2.fingerprint('migrate'), self.ir2.fingerprint('addon-nvim'))

    MAN = ("copy-nvim 100 aaaaaaaa Files/nvim/bin/nvim\ncopy-nvim 5 bbbbbbbb Files/nvim/share/README.txt\n"
           "copy-nvim 7 cccccccc Files/nvim/share/old.txt\ncopy-unifont 0 00000000 copy#Files/unifont#unifont\n"
           "- 0 11111111 install.dos#copy-nvim\n")

    def test_manifest_entries_keep_files_only(self):
        e = self.ir2.manifest_entries(self.MAN)
        self.assertEqual(sorted(e), ['Files/nvim/bin/nvim', 'Files/nvim/share/README.txt', 'Files/nvim/share/old.txt'])
        self.assertEqual(e['Files/nvim/bin/nvim'], ('100', 'aaaaaaaa'))

    def test_the_planted_file_is_one_both_kits_list_the_same_and_the_changed_files_are_the_others(self):
        old = self.ir2.manifest_entries(self.MAN)
        new = dict(old)
        new['Files/nvim/bin/nvim'] = ('101', 'dddddddd')
        new['Files/nvim/share/new.txt'] = ('3', 'eeeeeeee')
        self.assertEqual(self.ir2.unchanged_file(old, new, 'Files/nvim/'), 'Files/nvim/share/README.txt')
        self.assertEqual(self.ir2.changed_files(old, new, 'Files/nvim/'), ['Files/nvim/bin/nvim', 'Files/nvim/share/new.txt'])
        self.assertIsNone(self.ir2.unchanged_file(old, new, 'Files/python3/'))

    def test_the_legacy_kit_and_itself_share_a_file_to_plant_in(self):
        m = ir.LEGACY_KIT / 'Files/MANIFEST'
        if not m.exists(): self.skipTest('no legacy kit at %s' % ir.LEGACY_KIT)
        e = self.ir2.manifest_entries(m.read_text(errors='replace'))
        self.assertTrue(self.ir2.unchanged_file(e, e, 'Files/nvim/').startswith('Files/nvim/'))
        self.assertTrue(self.ir2.unchanged_file(e, e, 'Files/unifont/').startswith('Files/unifont/'))
        self.assertEqual(self.ir2.changed_files(e, e, 'Files/nvim/'), [])

    def test_the_drawer_page_is_what_tells_a_full_install_from_an_update(self):
        full = ['page [VTC:distkit/Install] ids 89 90 91 92', 'Proceed (id 90)']
        upd = ['page [VTC:distkit/Install] ids 1 2 90 91', 'Proceed (id 90)']
        self.assertTrue(self.ir2.asked_drawer(full))
        self.assertFalse(self.ir2.asked_drawer(upd))
        self.assertFalse(self.ir2.asked_drawer(['page [x] ids 90 920']))

    def test_the_installer_window_of_any_kit_is_judged_for_layout(self):
        for t in ('VTC:distkit/Install', 'VTC:kit-nvim/Install', 'VTC:legacykit/Install'):
            self.assertTrue(self.ir2.KITWINDOW.match(t), t)
        self.assertFalse(self.ir2.KITWINDOW.match('Make New Drawer'))

    def test_the_add_on_case_starts_the_add_ons_own_install_icon_after_the_base(self):
        import verdicts
        for pkg in ir.addon_ids():
            sim = ScriptedAmiga(self.pkgs)
            started = []
            def installer_run(tag, inst, kitdir, appname, level, base, dest=None):
                started.append((kitdir, appname))
                sim.run_long('runinstall' if kitdir == 'distkit' else 'runinstall-' + kitdir[len('kit-'):])
                return True, [], 0.0
            saved = (ir.run, ir.run_long, ir.run_slow, ir.prepare, self.ir2.installer_run, self.ir2.read_installer)
            ir.run, ir.run_long, ir.run_slow, ir.prepare = sim.run, sim.run_long, sim.run_slow, lambda dest, addons=(): None
            self.ir2.installer_run, self.ir2.read_installer = installer_run, lambda: ('Installer', set())
            try:
                ok, detail = self.ir2.run_addon_case(pkg)
            finally:
                ir.run, ir.run_long, ir.run_slow, ir.prepare, self.ir2.installer_run, self.ir2.read_installer = saved
            self.assertTrue(ok, (pkg, detail))
            self.assertEqual(started, [('distkit', 'UP-Term'), ('kit-' + pkg, ir.archive(pkg))], 'base first, then its own icon')

class NoBaseCase(unittest.TestCase):
    """The nobase case on a scripted Amiga: it drives every add-on's install.dos (call-count) and it fails
    when an add-on installs without the base."""
    def run_case(self, refuses):
        sys.path.insert(0, str(ROOT / 'tools'))
        import pkgs, verdicts
        ids = ir.based_ids()
        ran, files = [], {'S:User-Startup': 'u'}
        def run(cmd, timeout=60):
            if cmd.startswith('Type S:User-Startup'): return 0, files['S:User-Startup']
            if cmd.startswith('Type ENV:UPTCHECK'): return 0, 'up-nvim needs UP-Term 1.0 or newer. Install up-term.lha first.'
            if cmd.startswith('List >NIL: C:') or cmd.startswith('List >NIL: UP-Term:'): return (0 if ran and not refuses else 5), ''
            return 0, ''
        def run_long(script, timeout=1500):
            if script.startswith('runinstall-'): ran.append(script)
            return (20 if refuses else 0), ''
        saved = (ir.run, ir.run_long, ir.prepare, ir.Before)
        class FakeBefore: startup = 'u'
        ir.run, ir.run_long, ir.prepare, ir.Before = run, run_long, lambda dest, addons=(): None, FakeBefore
        ck = verdicts.Checks()
        try:
            ir.case_nobase(ck)
        finally:
            ir.run, ir.run_long, ir.prepare, ir.Before = saved
        return ck, ran, ids

    def test_it_tries_every_add_on_and_passes_when_each_is_refused_with_20(self):
        ck, ran, ids = self.run_case(refuses=True)
        self.assertEqual(ran, ['runinstall-' + i for i in ids])
        self.assertEqual(ck.failed, [])

    def test_it_fails_when_an_add_on_installs_without_the_base(self):
        ck, ran, ids = self.run_case(refuses=False)
        self.assertTrue(any('returns 20' in f for f in ck.failed), ck.failed)

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

class OwnersAddonsOnTheRig(unittest.TestCase):
    """The owner keeps Claude, amiga-pi and Python3 on the rigs; the 'X is not there before its Install' checks need
    them out. A fake rig state (records in ENVARC:upkg, the scripts the cases run): cli() takes the installed
    add-ons out with each one's own Uninstall before the first case and installs them again after the last."""
    def setUp(self):
        import contextlib, tempfile
        sys.path.insert(0, str(ROOT / 'tools'))
        import pkgs
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.rec = {r['id']: r['record'] for r in pkgs.lha_rows() if r['id'] != 'base'}
        self.installed = {'claude', 'pi', 'python'}
        self.log = []
        self.saved = {k: getattr(ir, k) for k in ('run', 'run_long', 'stage_addon', 'PREINSTALLED', 'VTC', 'case_table', 'kit_dir', 'show_close')}
        self.saved_rig, self.saved_lock = ir.paths.RIG, ir.paths.rig_lock
        self.startup = b'Echo hi\n'
        self.saved_ami = (ami.read_file, ami.put)
        ami.read_file = lambda p: self.startup
        ami.put = lambda local, remote: setattr(self, 'startup', open(local, 'rb').read())
        ir.paths.RIG = self.tmp
        ir.paths.rig_lock = contextlib.nullcontext
        ir.VTC, ir.PREINSTALLED = self.tmp, self.tmp / 'preinstalled.json'
        ir.run, ir.run_long, ir.stage_addon = self.fake_run, self.fake_long, lambda pkg: None
        ir.show_close = lambda: None
        ir.kit_dir = lambda pkg: self.tmp / 'kit'
        (self.tmp / 'kit').mkdir()
        (self.tmp / 'kit/Install').write_text('x')
    def tearDown(self):
        for k, v in self.saved.items(): setattr(ir, k, v)
        ir.paths.RIG, ir.paths.rig_lock = self.saved_rig, self.saved_lock
        ami.read_file, ami.put = self.saved_ami
    def own(self, fn):
        fn.own_fixtures = True   # no Fixtures: the fake rig has no files to save
        return fn
    def fake_run(self, cmd, timeout=60):
        for pkg, rec in self.rec.items():
            if cmd == 'List >NIL: %s/MANIFEST' % rec:
                return (0 if pkg in self.installed else 5), ''
        return 0, ''
    def fake_long(self, script, timeout=1500):
        self.log.append(script)
        if '-' not in script: return 0, ''
        kind, pkg = script.split('-', 1)
        (self.installed.discard if kind == 'rununinstall' else self.installed.add)(pkg)
        return 0, ''

    def test_installed_add_ons_are_taken_out_before_the_cases_and_put_back_after(self):
        seen = []
        def case(ck): seen.append(sorted(self.installed))
        ir.case_table = lambda: {'claude': (['claude'], [], self.own(case))}
        rc = ir.cli([])
        self.assertEqual(rc, 0)
        self.assertEqual(seen, [[]], 'a case ran with an add-on still on the rig')
        self.assertEqual(self.installed, {'claude', 'pi', 'python'}, 'the owner\'s add-ons are back')
        self.assertEqual(sorted(l for l in self.log if l.startswith('rununinstall')),
                         ['rununinstall-claude', 'rununinstall-pi', 'rununinstall-python'])
        self.assertEqual(sorted(l for l in self.log if l.startswith('runinstall')),
                         ['runinstall-claude', 'runinstall-pi', 'runinstall-python'])
        self.assertLess(max(i for i, l in enumerate(self.log) if l.startswith('rununinstall')),
                        min(i for i, l in enumerate(self.log) if l.startswith('runinstall')))
        self.assertFalse(ir.PREINSTALLED.exists())

    def test_the_rigs_own_up_term_assign_is_restored_before_the_add_ons_are_installed(self):
        import rig
        ir.case_table = lambda: {'claude': (['claude'], [], self.own(lambda ck: None))}
        ir.cli([])
        self.assertIn('restoreassign', self.log)
        self.assertLess(self.log.index('restoreassign'), self.log.index('runinstall-claude'))
        self.assertEqual((self.tmp / 'restoreassign').read_text(), rig.UPTERM_ASSIGN + '\n')
        self.assertIn('Assign >NIL: UP-Term: VTC:Apps', rig.GO, 'GO uses the same lines')

    def test_an_add_on_the_rig_does_not_hold_is_not_installed_afterwards(self):
        self.installed = {'pi'}
        ir.case_table = lambda: {'claude': (['claude'], [], self.own(lambda ck: None))}
        ir.cli([])
        self.assertEqual(self.installed, {'pi'})
        self.assertEqual(self.log, ['rununinstall-pi', 'restoreassign', 'runinstall-pi'])

    def test_the_add_ons_are_put_back_when_a_case_aborts(self):
        def case(ck): raise SystemExit('the agent is gone')
        ir.case_table = lambda: {'claude': (['claude'], [], self.own(case))}
        self.assertEqual(ir.cli([]), 2)
        self.assertEqual(self.installed, {'claude', 'pi', 'python'})

    def test_a_python3_block_its_uninstall_leaves_is_taken_out_for_the_cases(self):
        # Python3's Uninstall keeps ;BEGIN Python3 when S:User-Startup is not what its Install kept
        self.startup = b'Echo hi\n;BEGIN Python3\nAssign Python3: SYS:Python3\n;END Python3\n;BEGIN Other\nx\n;END Other\n'
        seen = []
        ir.case_table = lambda: {'python': (['python'], [], self.own(lambda ck: seen.append(self.startup)))}
        ir.cli([])
        self.assertEqual(seen, [b'Echo hi\n;BEGIN Other\nx\n;END Other\n'])

    def test_a_rig_with_no_add_on_runs_no_script(self):
        self.installed = set()
        ir.case_table = lambda: {'claude': (['claude'], [], self.own(lambda ck: None))}
        ir.cli([])
        self.assertEqual(self.log, [])


class UpkgRigPure(unittest.TestCase):
    """upkg_rig's pure parts: the Packages edits (the per-case feeds), the config, the bumped table, the records."""
    PACKAGES = "Package: Claude\nVersion: 1.1\nSHA256sum: aa\n\nPackage: up-nvim\nVersion: 1.0\nSHA256sum: bb\n"

    def test_edit_packages_changes_only_the_named_package(self):
        import upkg_rig as u
        out = u.edit_packages(self.PACKAGES, 'up-nvim', SHA256sum='cc', Version='1.1')
        self.assertEqual([(p['Package'], p['Version'], p['SHA256sum']) for p in u.parse_packages(out)],
                         [('Claude', '1.1', 'aa'), ('up-nvim', '1.1', 'cc')])
        with self.assertRaises(KeyError):
            u.edit_packages(self.PACKAGES, 'nosuch', Version='1')

    def test_conf_for_changes_only_the_src_line(self):
        import upkg_rig as u
        conf = (u.ROOT / 'dist/upkg.conf').read_text()
        out = u.conf_for(conf, 'http://127.0.0.1:5')
        a, b = conf.split('\n'), out.split('\n')
        self.assertEqual([x for x, y in zip(a, b) if x != y], [l for l in a if l.startswith('src/gz ')])
        self.assertIn('src/gz upterm http://127.0.0.1:5', b)
        self.assertEqual(len(a), len(b))
        with self.assertRaises(ValueError):
            u.conf_for('arch x 1\n', 'http://h')

    def test_bump_table_changes_the_version_of_one_row(self):
        import upkg_rig as u, pkgs
        out = u.bump_table(pkgs.TABLE.read_text(), 'nvim', '1.1')
        rows = {r['id']: r['version'] for r in pkgs.rows(__import__('pathlib').Path(self._write(out)))}
        self.assertEqual(rows['nvim'], '1.1')
        self.assertEqual(rows['python'], {r['id']: r['version'] for r in pkgs.rows()}['python'])
        with self.assertRaises(KeyError):
            u.bump_table(pkgs.TABLE.read_text(), 'nosuch', '1')

    def _write(self, text):
        import tempfile
        f = tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False)
        self.addCleanup(os.unlink, f.name)
        f.write(text)
        f.close()
        return f.name

    def test_status_version_and_lowered_record(self):
        import upkg_rig as u
        self.assertEqual(u.status_version(self.PACKAGES, 'up-nvim'), '1.0')
        self.assertIsNone(u.status_version(self.PACKAGES, 'Python3'))
        self.assertEqual(u.lower_record("up-nvim 1.0 abc d\nrest\n", '0.0.1'), "up-nvim 0.0.1 abc d\nrest\n")

if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
