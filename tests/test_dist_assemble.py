#!/usr/bin/env python3
"""tools/mkinstaller.py: the scripts of a package, assembled from dist/ and dist/common/.
(K2-3 proved the base's assembled Install, install.dos and Uninstall byte for byte equal
to the files before the shared parts moved out; K2-4 changed the base, so the copies went.)"""
import pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkinstaller as mi
import pkgs


class Assemble(unittest.TestCase):
    def test_every_name_in_the_base_scripts_has_a_value(self):
        for name in ('Install.installer', 'install.dos', 'Uninstall'):
            self.assertNotRegex(mi.build(ROOT / 'dist' / name, 'base'), r'@[A-Z0-9]+@', name)

    def test_the_templates_do_include_the_shared_parts(self):
        for name, frag in (('Install.installer', 'procs.installer'), ('install.dos', 'record.dos'),
                           ('Uninstall', 'uninstall-var.dos')):
            self.assertIn('; @include dist/common/%s\n' % frag, (ROOT / 'dist' / name).read_text(encoding='latin-1'))

    def _tmp(self, text):
        d = pathlib.Path(tempfile.mkdtemp())
        p = d / 't'
        p.write_text(text, encoding='latin-1')
        return p

    def test_an_unknown_name_stops_the_tool(self):
        with self.assertRaises(SystemExit) as e:
            mi.assemble(self._tmp('a @NOPE@ b\n'), {})
        self.assertIn('@NOPE@', str(e.exception))

    def test_an_include_that_includes_itself_stops_the_tool(self):
        p = self._tmp('x\n')
        p.write_text('; @include %s\n' % p, encoding='latin-1')
        with self.assertRaises(SystemExit) as e:
            mi.assemble(p, {}, ())
        self.assertIn('includes itself', str(e.exception))

    def test_values_fill_names_in_the_text(self):
        self.assertEqual(mi.sub('@ID@/@ARCHIVE@', {'ID': 'nvim', 'ARCHIVE': 'up-nvim'}), 'nvim/up-nvim')

    def test_an_add_on_gets_its_record_and_options_drawers_from_the_table(self):
        v = mi.read_values('nvim')
        self.assertEqual(v['RECORD'], 'UP-Term:pkg/up-nvim')
        self.assertEqual(v['OPTS'], 'UP-Term:pkg/up-nvim/opts')
        self.assertEqual((v['BASEMAJOR'], v['BASEMINOR']), ('1', '0'))
        self.assertEqual(mi.read_values('base')['OPTS'], 'ENVARC:up-term/opts')
        self.assertEqual(mi.read_values('nvim')['RECORDPRE'], 'record-addon.dos')
        self.assertEqual(mi.read_values('base')['RECORDPRE'], 'record-legacy.dos')
        self.assertEqual(mi.read_values('nvim')['RECORDP'], 'UP-Term:pkg/up-nvim/')

    def test_the_base_gets_the_add_ons_uninstall_blocks_and_search_lines_from_the_table(self):
        v = mi.read_values('base')
        # only the archive that needs the base is run from UP-Term:pkg; the stand-alone ones (ENVARC:upkg) are left alone
        self.assertEqual(v['ADDONUNINSTALL'],
                         '\n'.join('If EXISTS UP-Term:pkg/up-%s/Uninstall\n  Execute UP-Term:pkg/up-%s/Uninstall\nEndIf' % (i, i)
                                   for i in [r['archive'][3:] for r in pkgs.rows() if r['id'] != 'base' and r['record'].startswith('UP-Term:pkg/')]))
        # Files/Claude finds an old Files/ClaudeCode by substring too: one search covers both
        handed = {p for r in pkgs.rows() if r['id'] != 'base' and r['handed'] != '-' for p in r['handed'].split(',')}
        searched = {p for p in handed if not any(q != p and p.startswith(q) for q in handed)}
        self.assertEqual(v['HANDEDSEARCH'].count('Search >NIL: UP-Term:MANIFEST'), len(searched))
        for p in searched:
            self.assertIn('Search >NIL: UP-Term:MANIFEST " %s"' % p, v['HANDEDSEARCH'])
        self.assertNotIn('Files/ClaudeCode', v['HANDEDSEARCH'])
        self.assertEqual(mi.read_values('nvim')['ADDONUNINSTALL'], '')

    def test_an_add_ons_install_dos_makes_and_copies_through_one_real_drawer(self):
        # UP-Term: may be a multi-directory assign: MakeDir creates in its first directory only, where pkg/ may
        # not exist. The record drawer is made and filled through the drawer ENVARC:up-term/Dir names.
        for pid in ('git', 'nvim'):
            text = mi.build(ROOT / 'dist/addons' / pid / 'install.dos', pid)
            rec = mi.read_values(pid)['RECORD']
            self.assertIn('Assign >NIL: UPTREAL: "$uptdir"', text, pid)
            self.assertIn('MakeDir >NIL: $uptpkg $uptrec\n', text, pid)
            self.assertIn('Copy VERSIONS $uptrec/VERSIONS ', text, pid)
            self.assertIn('Set uptrec "UPTREAL:pkg/up-%s"' % pid, text, pid)
            for line in text.splitlines():
                if line.startswith(('MakeDir', 'Copy VERSIONS', 'Copy UPTPARENT')):
                    self.assertNotIn(rec, line, pid)

    def test_a_missing_or_empty_dir_file_target_never_reaches_the_assign_or_exists(self):
        # s25: Assign of a drawer that is gone returns rc 20, which stops a script under FailAt 10 before the
        # If NOT WARN fallback; and EXISTS "" asks for a volume. Both are looked at first.
        for pid in ('git', 'nvim'):
            text = mi.build(ROOT / 'dist/addons' / pid / 'install.dos', pid)
            i = text.index('Set uptdir "`Type ENVARC:up-term/Dir`"')
            tail = text[i:]
            self.assertLess(tail.index('If NOT "$uptdir" EQ ""'), tail.index('If EXISTS "$uptdir"'), pid)
            self.assertLess(tail.index('If EXISTS "$uptdir"'), tail.index('Assign >NIL: UPTREAL: "$uptdir"'), pid)
            self.assertIn('[WARN] ENVARC:up-term/Dir names $uptdir, which does not exist', tail, pid)
            self.assertNotIn('Assign >NIL: UPTREAL: "`Type', text, pid)

    def test_no_add_on_payload_write_goes_through_up_term_and_every_part_resolves_first(self):
        # s25 (rig 10, UP-Term: = RAM:emptyupt + Rushhours:Tools/UP-Term): up-git's Files landed in the first
        # directory. Every write (MakeDir, Copy into UP-Term:) names UPTREAL:, and the resolve runs before any STEP Skip.
        for pid in [r['id'] for r in pkgs.rows() if r['id'] not in ('base', 'uhc') and r['record'].startswith('UP-Term:')]:
            src = (ROOT / 'dist/addons' / pid / 'install.dos').read_text()
            for st in (l.strip() for l in src.splitlines()):
                self.assertNotRegex(st, r'^MakeDir (>NIL: )?"?UP-Term:', (pid, st))
                self.assertNotRegex(st, r'^Copy .* "?UP-Term:\S* ', (pid, st))
            self.assertIn('@include dist/common/@RECORDRESOLVE@', src, pid)
            self.assertLess(src.index('@include dist/common/@RECORDRESOLVE@'), src.index('Skip {STEP}'), pid)

    def test_an_add_on_uninstall_deletes_through_the_real_drawer(self):
        # s25: Delete "UP-Term:bin/git" finds nothing when the first directory of the assign has no bin/
        un = mi.build(ROOT / 'dist/common/uninstall-addon.dos', 'git')
        self.assertIn('Delete >NIL: "UPTREAL:bin/git" ALL QUIET', un)
        self.assertNotIn('Delete >NIL: "UP-Term:', un)
        self.assertLess(un.index('Assign >NIL: UPTREAL: "$uptdir"'), un.index('Delete >NIL: "UPTREAL:bin/git"'))
        self.assertIn('Delete >NIL: "$uptrec" ALL QUIET', un)
        self.assertLess(un.index('Delete >NIL: "$uptrec" ALL QUIET'), un.index('Assign >NIL: UPTREAL:\nEcho'))

    def test_the_postinst_and_prerm_of_an_add_on_use_no_rm_and_read_the_real_drawer(self):
        # s25: vsh has no rm ("vsh: rm: not found"); the prerm Copy of UP-Term:pkg/.. failed on a multi-directory assign
        for r in pkgs.addons():
            if r['record'].startswith('UP-Term:'):
                for keys in ((), ('K',)):
                    need, post, pre = pkgs._scripts(r, keys)
                    self.assertNotRegex(post, r'(?m)^rm\b')
                    self.assertIn('Delete >NIL: ', post)
                    self.assertIn('Assign >NIL: UPTREAL: "$d"', pre)
                    self.assertIn('Copy UPTREAL:pkg/%s/Uninstall T:' % r['archive'], pre)
                    self.assertNotIn('Copy UP-Term:', pre)

    def test_an_add_ons_manifest_and_options_land_in_the_real_drawer_of_a_multi_directory_assign(self):
        # rig 10, s15: with UP-Term: = TwoA + SYS:UP-Term the MANIFEST copy through UP-Term: failed rc 20 ("Can't
        # open UP-Term:pkg/up-git/MANIFEST for output") because UPTREAL: was gone before record.dos wrote it.
        for pid in ('git', 'nvim'):
            lines = [l.strip() for l in mi.build(ROOT / 'dist/addons' / pid / 'install.dos', pid).splitlines()]
            self.assertIn('Copy MANIFEST $uptrec/MANIFEST CLONE', lines, pid)
            self.assertIn('Copy ENV:UPTOPT#? $uptrec/opts CLONE QUIET', lines, pid)
            # UPTREAL: stays assigned until every record write is done, and is removed once after them
            last_write = max(i for i, l in enumerate(lines) if '$uptrec' in l)
            drops = [i for i, l in enumerate(lines) if l == 'Assign >NIL: UPTREAL:']
            self.assertTrue(any(i > last_write for i in drops), pid)
            self.assertFalse(any(l.startswith(('Copy', 'MakeDir', 'Delete')) and mi.read_values(pid)['RECORD'] in l
                                 for l in lines[lines.index('Lab record'):]), pid)
        # the base and a stand-alone archive keep their own record names
        base = mi.build(ROOT / 'dist' / 'install.dos', 'base')
        self.assertIn('Copy MANIFEST UP-Term:MANIFEST CLONE', base)
        self.assertNotIn('Assign >NIL: UPTREAL:', base)

    def test_a_stand_alone_package_gets_no_base_check_and_a_record_in_envarc(self):
        for pid in ('claude', 'pi', 'python'):
            v = mi.read_values(pid)
            self.assertEqual(v['STANDALONE'], 'YES', pid)
            self.assertEqual(v['CHECKFRAG'], 'check-standalone.dos', pid)
            self.assertTrue(v['RECORD'].startswith('ENVARC:upkg/'), pid)
            self.assertEqual(v['OPTS'], v['RECORD'] + '/opts')
            self.assertEqual(v['PKGDIR'], 'ENVARC:upkg')
            self.assertNotEqual(v['DESTTEXT'], '', pid)
        self.assertEqual(mi.read_values('nvim')['STANDALONE'], 'NO')
        self.assertEqual(mi.read_values('nvim')['CHECKFRAG'], 'check.dos')
        self.assertEqual(mi.read_values('base')['STANDALONE'], 'NO')
        self.assertEqual(mi.read_values('python')['LEGACY'], 'NO')
        self.assertEqual(mi.read_values('claude')['LEGACY'], 'YES')

    def test_an_unknown_package_stops_the_tool(self):
        with self.assertRaises(SystemExit):
            mi.read_values('nope')


def tree(src):
    """The Installer script as nested lists of words and ('str', text); comments gone."""
    stack, i = [[]], 0
    while i < len(src):
        c = src[i]
        if c == ';':
            j = src.find('\n', i)
            i = len(src) if j < 0 else j
        elif c == '(':
            stack.append([]); i += 1
        elif c == ')':
            done = stack.pop(); stack[-1].append(done); i += 1
        elif c == '"':
            j = i + 1
            while src[j] != '"':
                j += 2 if src[j] == '\\' else 1
            stack[-1].append(('str', src[i + 1:j])); i = j + 1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < len(src) and not src[j].isspace() and src[j] not in '()";':
                j += 1
            stack[-1].append(src[i:j]); i = j
    return stack[0]


class FirstInstall(unittest.TestCase):
    """With no UP-Term: assign (a first install), any (exists "UP-Term:...") makes AmigaOS put
    up "Please insert volume UP-Term" before the welcome page. Every probe of a UP-Term: path
    (or of #oldman, the record's MANIFEST in UP-Term:) must sit inside an if that asks
    getassign "UP-Term" first."""

    PROBES = ('exists', 'getsize', 'getversion', 'getsum')


    class Welcome(Exception):
        pass

    def _unguarded(self, node, guarded, out):
        """Probes in node with no getassign guard, in script order up to the first page."""
        if not isinstance(node, list) or not node:
            return
        head = node[0]
        if head == 'message':
            raise self.Welcome
        if head in self.PROBES and len(node) > 1:
            arg = node[1]
            hits = arg == '#oldman' or (isinstance(arg, tuple) and arg[1].startswith('UP-Term:'))
            if hits and not guarded:
                out.append(repr(node)[:120])
        if head == 'if' and len(node) > 2:
            asks = 'getassign' in repr(node[1]) and "'UP-Term'" in repr(node[1])
            self._unguarded(node[1], guarded, out)
            for branch in node[2:]:
                self._unguarded(branch, guarded or asks, out)
            return
        for child in node:
            self._unguarded(child, guarded, out)

    def test_a_first_install_asks_for_no_up_term_volume_before_the_welcome(self):
        for template, pkg in (('Install.installer', 'base'), ('addons/nvim/Install.installer', 'nvim'),
                              ('addons/claude/Install.installer', 'claude'),
                              ('addons/pi/Install.installer', 'pi'),
                              ('addons/python/Install.installer', 'python')):
            out = []
            with self.assertRaises(self.Welcome, msg=pkg):
                self._unguarded(tree(mi.build(ROOT / 'dist' / template, pkg)), False, out)
            self.assertEqual(out, [], pkg)

if __name__ == '__main__':
    unittest.main()
