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

    def test_a_stand_alone_archive_never_names_an_up_term_object_outside_comments(self):
        """K2-1, owner 2026-10-13: Claude, amiga-pi and Python3 install on a Workbench with no UP-Term. An
        AmigaDOS object named UP-Term:... with no such assign would put up a "Please insert volume" requester,
        so the only lines that may name one sit behind a check of the assign (Assign >NIL: UP-Term: EXISTS)."""
        for r in ADDONS:
            if r['needs-base'] != '-':
                continue
            s = scripts(r['id'])
            for kind, text in s.items():
                guarded = False
                for line in text.splitlines():
                    code = line.split(';', 1)[0] if line.lstrip().startswith(';') else line
                    if 'Assign >NIL: UP-Term: EXISTS' in line:
                        guarded = True
                    if line.lstrip().startswith(';') or kind == 'installer' and 'UP-Term' in line and '"' in line:
                        continue
                    if re.search(r'\bUP-Term:', code) and not guarded:
                        self.fail('%s %s: %s' % (r['id'], kind, line))
            self.assertNotIn('UP-Term.version', s['dos'], r['id'])
            self.assertNotIn('UP-Term:pkg/', s['dos'], r['id'])

    def test_a_stand_alone_archive_records_itself_in_envarc_uppkg(self):
        for r in ADDONS:
            if r['needs-base'] == '-':
                self.assertEqual(r['record'], 'ENVARC:uppkg/' + r['archive'])
                inst = scripts(r['id'])['installer']
                self.assertIn('(set #dest (if (= "YES" "YES")', inst)
                self.assertIn('ENVARC:uppkg/%s/MANIFEST' % r['archive'], inst)

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
                if name.startswith('copy-') and 'UP-Term:' in body:
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
            if row['needs-base'] == '-':
                # stand-alone: nothing to check, no look at UP-Term:
                self.assertNotIn('Quit 20', check)
                self.assertNotRegex(check, r'(?m)^[^;\n]*UP-Term')
                continue
            self.assertIn('Echo >ENV:UPTCHECK "%s needs UP-Term %s or newer. Install UP-Term.lha first."' % (row['archive'], row['needs-base']), check)
            major, minor = row['needs-base'].split('.')
            self.assertIn('Version >NIL: FILE UP-Term:UP-Term.version %s %s' % (major, minor), check)
            self.assertIn('Version >NIL: LIBS:ixemul.library %s' % row['ixemul'], check)
            self.assertEqual(check.count('Quit 20'), 3)

    def test_the_record_part_makes_the_record_drawer_and_keeps_the_uninstall(self):
        for pid, s in self.each():
            row = {r['id']: r for r in ADDONS}[pid]
            rec = dict(parts_of(s['dos']))['record']
            self.assertIn('MakeDir >NIL: %s %s\n' % (row['record'].rsplit('/', 1)[0], row['record']), rec)
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
            # Python3 takes nothing over from the single kit (its copy is in UP-Term:Python3)
            adopts = '(= "YES" "YES")' in s['installer'].split('"UP-Term:pkg/legacy/MANIFEST"')[0][-400:]
            self.assertEqual(adopts, pid != 'python', pid)

    def test_every_file_parts_txt_names_has_a_part_of_install_dos(self):
        for pid, s in self.each():
            parts = mm.read_parts((ROOT / 'dist/addons' / pid / 'parts.txt').read_text())
            labs = {n for n, _ in parts_of(s['dos'])}
            self.assertEqual({p for p, _, _ in parts} - labs - {'-'}, set(), pid)
            for p, k, d in parts:
                if p.startswith('copy-') and 'UP-Term:' in dict(parts_of(s['dos']))[p]:
                    self.assertIsNotNone(d)        # a copy part of the Installer's copyfiles/Update names its drawer
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
            self.assertEqual(lines[-3], 'Delete >NIL: "%s" ALL QUIET' % row['record'])
            self.assertEqual(lines[-2], 'Delete >NIL: "%s" QUIET' % row['record'].rsplit('/', 1)[0])   # an empty drawer only

    def test_the_base_runs_the_uninstall_of_the_add_ons_that_need_it_and_leaves_the_others(self):
        base = mi.build(ROOT / 'dist/Uninstall', 'base')
        for r in ADDONS:
            if r['needs-base'] == '-':
                self.assertNotIn(r['record'], base.replace('ENVARC:uppkg/Claude\n', '').replace('ENVARC:uppkg/amiga-pi\n', '')
                                 .replace('ENVARC:uppkg/Python3\n', ''), r['id'])
            else:
                self.assertIn('Execute %s/Uninstall' % r['record'], base)

    def test_the_base_uninstall_names_the_stand_alone_records_packages_txt_names(self):
        """The base Uninstall asks three questions of ENVARC:uppkg: the names in it are packages.txt's"""
        base = mi.build(ROOT / 'dist/Uninstall', 'base')
        names = set(re.findall(r'EXISTS ENVARC:uppkg/(\S+)', base))
        want = {r['record'].split('/')[-1] for r in ADDONS if r['needs-base'] == '-'}
        self.assertEqual(names, want)


class PythonUninstall(unittest.TestCase):
    S = scripts('python')

    def test_python_takes_out_only_its_own_block_its_assign_and_its_drawer(self):
        un = self.S['uninstall']
        self.assertIn('Assign Python3:\n', un)
        self.assertIn('Delete >NIL: SYS:Python3 ALL QUIET', un)
        self.assertNotIn('UP-Term:Python3', un)
        # the block goes by UP-Term's unstartup.sh when UP-Term is there (guarded: no UP-Term, no look at UP-Term:bin) ...
        v = un.index('UP-Term:bin/vsh ENVARC:up-term/unstartup.sh Python3')
        self.assertLess(un.index('Assign >NIL: UP-Term: EXISTS', un.index('Search >NIL: S:User-Startup')), v)
        # ... else by restoring the S:User-Startup Install kept, only when it is still that plus the block
        self.assertNotIn('Compare ', un.replace('no C:Compare', ''), 'a stock Workbench has no C:Compare (rig 2, 2026-10-13)')
        self.assertIn('If "$uspsize" EQ "$usnsize"', un)
        self.assertIn('Copy ENVARC:uppkg/Python3/User-Startup.before S:User-Startup CLONE QUIET', un)
        self.assertLess(v, un.index('If "$uspsize" EQ "$usnsize"'))
        # ... else it stays, and says so
        self.assertIn('S:User-Startup changed since Python3 was installed', un)

    def test_python_install_writes_the_block_the_uninstall_removes(self):
        dos = self.S['dos']
        block = [';BEGIN Python3', 'If EXISTS SYS:Python3', '  Assign Python3: SYS:Python3', 'EndIf', ';END Python3']
        for line in block:
            self.assertIn('Echo >>S:User-Startup "%s"' % line, dos)
            self.assertIn('Echo >>T:User-Startup.python "%s"' % line, self.S['uninstall'])
        # a name the base's unstartup.sh (every ";BEGIN UP-Term...") does not take with it
        self.assertNotIn('BEGIN UP-Term', dos)
        self.assertIn('Copy S:User-Startup ENVARC:uppkg/Python3/User-Startup.before CLONE QUIET', dos)

    def test_the_ixemul_part_installs_only_when_none_that_new_is_there_and_never_downgrades(self):
        """THE IXEMUL.LIBRARY RULE: a library of 47 or newer stays (UP-Term's 80 included)."""
        ix = dict(parts_of(self.S['dos']))['ixemul']
        a = ix.index('Version >NIL: LIBS:ixemul.library 47\nIf WARN\n')
        copy = ix.index('Copy libs/ixemul.library LIBS:ixemul.library CLONE QUIET')
        end = ix.index('EndIf', copy)
        self.assertLess(a, copy)
        self.assertEqual(ix.count('Copy libs/ixemul.library'), 1)        # one copy, inside the If WARN
        self.assertNotIn('\nEndIf', ix[a:copy])                          # nothing closes the If before the copy
        self.assertIn('Echo >ENVARC:uppkg/ixemul/installed-by-Python3', ix[copy:end])
        self.assertIn('Copy libs/ixnet.library LIBS:ixnet.library CLONE QUIET', ix[copy:end])
        # an older library is kept before it is replaced, and the need is recorded either way
        self.assertLess(ix.index('Copy LIBS:ixemul.library LIBS:ixemul.library.before-Python3 CLONE QUIET'), copy)
        self.assertIn('Echo >ENVARC:uppkg/ixemul/needs/Python3', ix[end:])
        self.assertEqual(self.S['dos'].count('LIBS:ixemul.library 47'), 1)

    def test_uninstall_removes_the_library_only_when_it_installed_it_and_nobody_needs_it(self):
        un = self.S['uninstall']
        a = un.index('Delete >NIL: ENVARC:uppkg/ixemul/needs/Python3 QUIET')
        rule = un[a:]
        for need in ('If EXISTS ENVARC:uppkg/ixemul/installed-by-Python3',        # this archive put it there
                     'Assign >NIL: UP-Term: EXISTS\n  If WARN',                    # UP-Term not installed
                     'Delete >NIL: ENVARC:uppkg/ixemul/needs QUIET',                # an empty drawer only goes
                     'If NOT EXISTS ENVARC:uppkg/ixemul/needs'):                    # ...so this says nobody else needs it
            self.assertIn(need, rule)
        order = [rule.index(x) for x in ('installed-by-Python3', 'UP-Term: EXISTS', 'ixemul/needs QUIET',
                                         'If NOT EXISTS ENVARC:uppkg/ixemul/needs', 'Delete >NIL: LIBS:ixemul.library LIBS:ixnet.library QUIET')]
        self.assertEqual(order, sorted(order))
        self.assertIn('Rename LIBS:ixemul.library.before-Python3 LIBS:ixemul.library', rule)
        self.assertNotIn('ALL', rule.split('Delete >NIL: ENVARC:uppkg/ixemul/needs QUIET')[1].split('\n')[0])

    def test_the_installer_runs_ixemul_before_the_copy_and_the_block(self):
        inst = self.S['installer']
        steps = re.findall(r'\(set #step "([a-z-]+)"', inst)
        self.assertEqual(steps, ['ixemul', 'copy-python', 'python', 'record'])


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
        # C:uptelnet comes with it (ClaudeCode needs it) and goes with it only when UP-Term is not installed
        self.assertIn('Copy uptelnet C:uptelnet CLONE QUIET', dos)
        un = self.S['uninstall']
        self.assertIn('Assign >NIL: UP-Term: EXISTS\nIf WARN\n  Delete >NIL: C:uptelnet QUIET', un)


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


class NvimShell(unittest.TestCase):
    """N1: nvim's default 'shell' is sh, which AmigaOS has not, so :terminal and :! failed with
    "E475 ... 'sh' is not executable". The archive's sysinit.vim ($VIM/sysinit.vim, read before
    the user's init) sets it to the base's vsh, and make dist-nvim stages it where nvim reads it."""

    def test_sysinit_sets_shell_to_the_bases_vsh(self):
        vim = (ROOT / 'dist/addons/nvim/sysinit.vim').read_text()
        code = [l.strip() for l in vim.splitlines() if not l.strip().startswith('"')]
        self.assertIn('set shell=/UP-Term/bin/vsh', code)
        self.assertIn('set shell=/c/vsh', code)
        # ixemul's Unix form of the base's UP-Term:bin/vsh and C:vsh (nvim's executable('C:vsh') is 0 on the rig)
        # the base installs vsh in both places (dist/install.dos: Copy vsh UP-Term:bin/vsh, C:vsh)
        dos = (ROOT / 'dist/install.dos').read_text()
        self.assertIn('Copy vsh UP-Term:bin/vsh', dos)
        self.assertIn('Copy vsh C:vsh', dos)

    def test_a_shell_the_user_named_is_not_replaced(self):
        vim = (ROOT / 'dist/addons/nvim/sysinit.vim').read_text()
        self.assertIn("empty(getenv('SHELL'))", vim)

    def test_the_kit_stages_sysinit_where_nvim_reads_it(self):
        mk = (ROOT / 'Makefile').read_text()
        m = re.search(r'(?ms)^dist-nvim:.*?^dist-python:', mk)
        self.assertIn('dist/addons/nvim/sysinit.vim $(KIT_nvim)/Files/nvim/share/nvim/sysinit.vim', m.group(0))


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
