#!/usr/bin/env python3
"""The add-on archives' scripts (dist/addons/<id>/, dist/common/), assembled by tools/mkinstaller.py,
for every add-on of dist/packages.txt, on the host (kit-split K2-6.5). The same checks as the base's
(tests/test_dist_installer.py): Installer 43.3 language, page text within 56 characters, the Installer
runs install.dos's parts in order, every part ends with the STEP quit. Added: the check part comes first and
stops the install, the record part is last, each Uninstall and parts.txt hold together."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests"))
import mkinstaller as mi
import mkmanifest as mm
import pkgs
import test_dist_installer as ti

ADDONS = [r for r in pkgs.rows() if r['id'] != 'base']


def scripts(pkg_id):
    a = ROOT / 'dist/addons' / pkg_id
    return {'installer': mi.build(a / 'Install.installer', pkg_id),
            'dos': mi.build(a / 'install.dos', pkg_id),
            'uninstall': mi.build(ROOT / 'dist/common/uninstall-addon.dos', pkg_id)}


def parts_of(dos):
    chunks = re.split(r'(?m)^Lab (\S+)\n', dos)
    return list(zip(chunks[1::2], chunks[2::2]))


class EveryAddOn(unittest.TestCase):
    def each(self):
        return [(r['id'], scripts(r['id'])) for r in ADDONS]

    def test_there_are_four_add_ons(self):
        self.assertEqual(sorted(r['id'] for r in ADDONS), ['claude', 'nvim', 'pi', 'python'])

    def test_no_name_is_left_without_a_value(self):
        for _, s in self.each():
            for text in s.values():
                self.assertNotRegex(text, r'@[A-Z0-9]+@')

    def test_parentheses_balance_and_strings_fit_43(self):
        for _, s in self.each():
            depth = 0
            for kind, text in ti.tokens(s['installer']):
                depth += {'open': 1, 'close': -1}.get(kind, 0)
                self.assertGreaterEqual(depth, 0)
                if kind == 'str':
                    self.assertLessEqual(len(text.encode('latin-1')), 512, text[:60])
            self.assertEqual(depth, 0)

    def test_page_text_lines_fit_the_32_window(self):
        for _, s in self.each():
            stack, long = [], []
            toks = ti.tokens(s['installer'])
            for i, (kind, text) in enumerate(toks):
                if kind == 'open':
                    stack.append(toks[i + 1][1] if i + 1 < len(toks) and toks[i + 1][0] == 'word' else '')
                elif kind == 'close':
                    stack.pop()
                elif kind == 'str' and 'help' not in stack and 'abort' not in stack:
                    long += [p for p in text.split('\\n') if len(p) > 56]
            self.assertEqual(long, [])

    def test_only_installer_43_language(self):
        for _, s in self.each():
            toks = ti.tokens(s['installer'])
            procs = {toks[i + 2][1] for i in range(len(toks) - 2)
                     if toks[i][0] == 'open' and toks[i + 1] == ('word', 'procedure')}
            used = {toks[i + 1][1] for i in range(len(toks) - 1) if toks[i][0] == 'open' and toks[i + 1][0] == 'word'}
            self.assertFalse(used & ti.LATER)
            self.assertEqual(used - ti.KNOWN_43 - procs, set())

    def test_the_installer_runs_check_first_then_every_part_in_order(self):
        for pid, s in self.each():
            parts = [n for n, _ in parts_of(s['dos'])]
            self.assertEqual(parts[0], 'check')
            self.assertEqual(parts[-1], 'record')
            want = []
            for name, body in parts_of(s['dos']):
                if name == 'check':
                    continue            # check.installer runs it, before any page
                if name.startswith('copy-'):
                    m = re.findall(r'(?m)^\s*Copy (\S+) UP-Term:(\S+) ALL CLONE QUIET$', body)
                    self.assertEqual(len(m), 1, name)
                    want.append(('copy', m[0][0], m[0][1]))
                else:
                    want.append(('step', name))
            got = []
            for m in re.finditer(r'\(set #step "([a-z-]+)"|\(set #from \(tackon #files "([^"]+)"\) #to \(tackon #dest "([^"]+)"\)',
                                 s['installer']):
                got.append(('step', m.group(1)) if m.group(1) else ('copy', m.group(2), m.group(3)))
            self.assertEqual(got, want)
            self.assertIn('"STEP=check"', s['installer'])
            self.assertLess(s['installer'].index('"STEP=check"'), s['installer'].index('(P_OPTIONS)\n'))
            self.assertLess(s['installer'].index('"STEP=check"'), s['installer'].index('(askstring') if '(askstring' in s['installer'] else 10 ** 9)

    def test_each_part_ends_with_the_step_quit_and_if_endif_pair(self):
        end = 'If NOT "{STEP}" EQ ""\n  Quit 0\nEndIf\n'
        for _, s in self.each():
            for name, body in parts_of(s['dos'])[:-1]:
                self.assertTrue(body.endswith(end), name)
            for name, body in parts_of(s['dos']):
                self.assertEqual(len(re.findall(r'(?m)^\s*If\b', body)), len(re.findall(r'(?m)^\s*EndIf\b', body)), name)
            self.assertIn('If NOT "{STEP}" EQ ""\n  Skip {STEP}\nEndIf\nLab check\n', s['dos'])

    def test_the_keywords_are_dir_step_and_remote_for_claude_only(self):
        for pid, s in self.each():
            keys = re.match(r'\.KEY (\S+)', s['dos']).group(1)
            self.assertEqual(keys, 'DIR/A,STEP/K,REMOTE/K' if pid == 'claude' else 'DIR/A,STEP/K')
            self.assertEqual(sorted(re.findall(r'\{(\w+)\}', s['dos'].replace('{STEP}', '').replace('{DIR}', ''))),
                             ['REMOTE'] if pid == 'claude' else [])

    def test_the_step_command_line_fits_255_characters(self):
        for _, s in self.each():
            line = ti.step_command_line('Work:' + 'k' * 49 + '/Files', 'D' * 40, s['installer'])
            self.assertLess(len(line), 255, line)

    def test_the_check_part_names_the_base_and_the_ixemul_this_add_on_needs(self):
        for pid, s in self.each():
            row = {r['id']: r for r in ADDONS}[pid]
            check = dict(parts_of(s['dos']))['check']
            self.assertIn('Echo >ENV:UPTCHECK "%s needs UP-Term %s or newer. Install UP-Term.lha first."' % (row['archive'], row['needs-base']), check)
            major, minor = row['needs-base'].split('.')
            self.assertIn('Version >NIL: FILE UP-Term:UP-Term.version %s %s' % (major, minor), check)
            self.assertIn('If "%s" EQ "80"\n  Version >NIL: LIBS:ixemul.library 80' % row['ixemul'], check)
            self.assertEqual(check.count('Quit 20'), 3)

    def test_the_record_part_makes_the_record_drawer_and_keeps_the_uninstall(self):
        for pid, s in self.each():
            row = {r['id']: r for r in ADDONS}[pid]
            rec = dict(parts_of(s['dos']))['record']
            self.assertIn('MakeDir >NIL: UP-Term:pkg %s\n' % row['record'], rec)
            self.assertIn('Copy MANIFEST %s/MANIFEST CLONE' % row['record'], rec)
            self.assertIn('Copy /Uninstall %s/Uninstall CLONE QUIET' % row['record'], rec)
            self.assertIn('Copy VERSIONS %s/VERSIONS CLONE QUIET' % row['record'], rec)
            self.assertIn('Delete >NIL: %s/opts ALL QUIET' % row['record'], rec)

    def test_the_installer_reads_the_legacy_manifest_through_only(self):
        for pid, s in self.each():
            row = {r['id']: r for r in ADDONS}[pid]
            self.assertIn('UP-Term:pkg/legacy/MANIFEST', s['installer'])
            self.assertIn('ONLY=%s' % row['handed'], s['installer'])
            self.assertIn('%s/MANIFEST' % row['record'], s['installer'])

    def test_every_file_parts_txt_names_has_a_part_of_install_dos(self):
        for pid, s in self.each():
            parts = mm.read_parts((ROOT / 'dist/addons' / pid / 'parts.txt').read_text())
            labs = {n for n, _ in parts_of(s['dos'])}
            self.assertEqual({p for p, _, _ in parts} - labs - {'-'}, set(), pid)
            for p, k, d in parts:
                if p.startswith('copy-'):
                    self.assertIsNotNone(d)
            handed = {h for h in {r['id']: r for r in ADDONS}[pid]['handed'].split(',')}
            kit = {k for _, k, _ in parts}
            self.assertEqual(handed - kit, set(), '%s: handed paths the archive does not list' % pid)

    def test_uninstall_removes_the_record_last_and_runs_its_body_first(self):
        for pid, s in self.each():
            row = {r['id']: r for r in ADDONS}[pid]
            un = s['uninstall']
            self.assertLess(un.index('FailAt 21'), un.index('Delete >NIL: "%s" ALL QUIET' % row['record']))
            lines = [l for l in un.splitlines() if l.strip() and not l.startswith(';')]
            self.assertTrue(lines[-1].startswith('Echo "%s removed' % row['archive']))
            self.assertEqual(lines[-2], 'Delete >NIL: "%s" ALL QUIET' % row['record'])

    def test_the_base_runs_the_uninstall_each_add_on_keeps(self):
        base = mi.build(ROOT / 'dist/Uninstall', 'base')
        for r in ADDONS:
            self.assertIn('Execute %s/Uninstall' % r['record'], base)


class PythonUninstall(unittest.TestCase):
    def test_python_takes_out_only_its_own_block_and_its_assign(self):
        un = scripts('python')['uninstall']
        self.assertIn('unstartup.sh python', un)
        self.assertIn('Assign Python3:\n', un)
        self.assertIn('Delete >NIL: UP-Term:Python3 ALL QUIET', un)

    def test_python_install_writes_the_block_the_uninstall_removes(self):
        dos = scripts('python')['dos']
        self.assertIn('Echo >>S:User-Startup ";BEGIN UP-Term python"', dos)
        self.assertIn('Echo >>S:User-Startup ";END UP-Term python"', dos)


class ClaudeInstall(unittest.TestCase):
    """CLAUDE-REMOTE-KEPT (moved from the base's suite): the Installer's server page starts with the host and
    port of an existing ENVARC:Claude/remote, and install.dos writes the chosen value."""
    S = scripts('claude')

    def test_page_default_is_the_existing_files_host_line(self):
        inst = self.S['installer']
        self.assertIn('(default #oldremote)', inst)
        self.assertNotIn('192.168.', inst, 'no private LAN address as a default')
        self.assertIn('REMOTE-ADDRESS >ENV:UPTermRemote', inst)
        self.assertIn('Stack 32768', inst, 'Claude refuses a stack under 16000')
        self.assertNotIn('Search ENVARC:Claude/remote', inst)
        self.assertLess(inst.index('(exists "ENVARC:Claude/remote")'), inst.index('(set #remote\n'))
        a = inst.index('(set #oldremote (getenv "UPTermRemote"))')
        self.assertIn('(= (substr #oldremote (- (strlen #oldremote) 1) 1) "\\n")', inst[a:a + 400])

    def test_install_writes_the_chosen_remote_even_when_the_file_exists(self):
        dos = self.S['dos']
        a = dos.index('If NOT "$optremote" EQ ""')
        blk = dos[a:dos.index('If NOT "{STEP}" EQ ""', a)]
        self.assertNotIn('If NOT EXISTS ENVARC:Claude/remote', blk)
        lines = [l.strip() for l in blk.splitlines()]
        want = ['Echo >ENVARC:Claude/remote "; Claude Code on another computer: host and port. With no API key,"',
                'Echo >>ENVARC:Claude/remote "; plain Claude connects there (uptelnet). Delete this file to stop."',
                'Echo >>ENVARC:Claude/remote "$optremote"']
        self.assertEqual([lines.index(w) for w in want], sorted(lines.index(w) for w in want))
        self.assertIn('Copy ENVARC:Claude/remote ENV:Claude/remote CLONE QUIET', lines)

    def test_the_binaries_and_the_icon_script(self):
        dos = self.S['dos']
        self.assertIn('Copy Claude C:Claude CLONE QUIET', dos)
        self.assertIn('Copy ClaudeCode C:ClaudeCode CLONE QUIET\nProtect C:ClaudeCode +s QUIET', dos)
        self.assertIn(' C:Claude C:ClaudeCode ', self.S['uninstall'])


class PiInstall(unittest.TestCase):
    """amiga-pi (moved from the base's suite): the binary to C:, ENVARC:amiga-pi made when missing, no settings
    file written; Uninstall removes the binary and leaves the drawer."""
    S = scripts('pi')

    def test_the_binary_and_the_settings_drawer(self):
        self.assertIn('Copy amiga-pi C:amiga-pi CLONE QUIET\nIf NOT EXISTS ENVARC:amiga-pi\n  MakeDir ENVARC:amiga-pi\nEndIf', self.S['dos'])

    def test_uninstall_removes_the_binary_not_the_settings_drawer(self):
        un = self.S['uninstall']
        self.assertIn('Delete >NIL: C:amiga-pi QUIET', un)
        self.assertNotRegex(un, r'(?m)^\s*Delete[^;\n]*ENVARC:amiga-pi')


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
