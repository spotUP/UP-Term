#!/usr/bin/env python3
"""The kit's Install (dist/Install.installer) against Files/install.dos, on the
host: nothing here runs an Installer (tools/rig/installer_rig.py does, on the
rig). Checked:

- install.dos is the one list of what gets installed: its parts (Lab lines)
  are run by the Installer one by one with STEP=<part>, in the same order,
  and its copy-<part> parts (one drawer copy each) are the drawers the
  Installer copies itself with copyfiles, same source, same destination;
- every part of install.dos ends by quitting when STEP named it, so a STEP
  run does that part only, and the parts are reached by Skip {STEP};
- every keyword of install.dos's .KEY line (but DIR and STEP) is passed by
  the Installer;
- the script is Installer 43.3 language (AmigaOS 3.1's Installer): balanced
  parentheses, no string longer than the 512 bytes 43.3 takes, and only
  statements, functions and parameters its guide (Installer.guide 1.19,
  Aminet util/misc/Installer-43_3.lha) documents -- none of the OS 3.5/3.9
  additions (showmedia, effect, ...), which 43.3 does not know and the
  Installer 47 of AmigaOS 3.2 ignores."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mkinstaller as mi
INSTALLER = mi.build(ROOT / "dist/Install.installer", "base")
DOS = mi.build(ROOT / "dist/install.dos", "base")

# Installer.guide 1.19 (43.3): statements, functions, parameters
KNOWN_43 = set("""
set symbolset makedir copyfiles copylib startup tooltype textfile execute run
rexx makeassign rename delete protect complete message working welcome if
select while until foreach abort exit trap onerror user debug procedure
cat substr strlen transcript tackon fileonly pathonly expandpath askdir
askfile askstring asknumber askchoice askoptions askbool askdisk exists
earlier getsize getdevice getdiskspace getsum getversion getenv getassign
iconinfo database patmatch symbolval and or xor not bitand bitor bitxor
bitnot shiftleft shiftright in
prompt help default choices newpath disk assigns all pattern files infos
noposition fonts nogauge optional delopts confirm safe source dest newname
command append include settooltype setdefaulttool setstack noreq swapcolors
resident quiet range override
= <> < > <= >= + - * /""".split())
LATER = {"showmedia", "closemedia", "setmedia", "effect", "openwbobject",
         "showwbobject", "closewbobject", "reboot", "back", "trace", "retrace",
         "querydisplay"}


def tokens(src):
    """(kind, text) for the script: 'open', 'close', 'str', 'word'; comments gone."""
    out, i = [], 0
    while i < len(src):
        c = src[i]
        if c == ';':
            i = src.find('\n', i) if '\n' in src[i:] else len(src)
        elif c in '()':
            out.append(('open' if c == '(' else 'close', c)); i += 1
        elif c == '"':
            j, buf = i + 1, ''
            while src[j] != '"':
                if src[j] == '\\':
                    buf += src[j:j + 2]; j += 2
                else:
                    buf += src[j]; j += 1
            out.append(('str', buf)); i = j + 1
        elif c.isspace():
            i += 1
        else:
            m = re.match(r'[^\s()";]+', src[i:])
            out.append(('word', m.group(0))); i += len(m.group(0))
    return out


def dos_parts():
    """install.dos's parts in order: [(name, body)]."""
    parts = re.split(r'(?m)^Lab (\S+)\n', DOS)
    return list(zip(parts[1::2], parts[2::2]))


class InstallerScript(unittest.TestCase):
    def test_parentheses_balance_and_strings_fit_43(self):
        depth = 0
        for kind, text in tokens(INSTALLER):
            depth += {'open': 1, 'close': -1}.get(kind, 0)
            self.assertGreaterEqual(depth, 0, 'a ) without its (')
            if kind == 'str':
                self.assertLessEqual(len(text.encode('latin-1')), 512, 'string over 512 bytes: %.60s' % text)
        self.assertEqual(depth, 0, 'unbalanced parentheses')

    def test_page_text_lines_fit_the_32_window(self):
        """INSTALLER-TEXT-WRAPS-ON-32: on 3.2's 560x176 Installer window (PAL Workbench) the screen font
        is wider than 3.1's, and a 60-character line wraps: the welcome's three such lines made 14 lines
        17, which ran under the buttons and left a cut line at the window's bottom (installer_rig's
        text check, 2026-10-08). Every line of page text is 56 characters or less; Help texts scroll
        in their own requester and are left out."""
        stack, long = [], []
        toks = tokens(INSTALLER)
        for i, (kind, text) in enumerate(toks):
            if kind == 'open':
                stack.append(toks[i + 1][1] if i + 1 < len(toks) and toks[i + 1][0] == 'word' else '')
            elif kind == 'close':
                stack.pop()
            elif kind == 'str' and 'help' not in stack and 'abort' not in stack:
                long += [part for part in text.split('\\n') if len(part) > 56]
        self.assertEqual(long, [])

    def test_only_installer_43_language(self):
        toks = tokens(INSTALLER)
        procs = {toks[i + 2][1] for i in range(len(toks) - 2)
                 if toks[i][0] == 'open' and toks[i + 1] == ('word', 'procedure')}
        used = {toks[i + 1][1] for i in range(len(toks) - 1)
                if toks[i][0] == 'open' and toks[i + 1][0] == 'word'}
        self.assertFalse(used & LATER, 'OS 3.5+ Installer statements: %s' % (used & LATER))
        self.assertEqual(used - KNOWN_43 - procs, set(), 'not in the Installer 43.3 guide')
        self.assertTrue(all(p.startswith('P_') for p in procs), 'procedures are named P_ (the guide)')


class OneListOfParts(unittest.TestCase):
    def test_installer_runs_every_part_in_order_and_copies_the_copy_parts(self):
        parts = [n for n, _ in dos_parts()]
        self.assertEqual(parts[0], 'drawer')
        want = []
        for name, body in dos_parts():
            if name.startswith('copy-'):
                m = re.findall(r'(?m)^\s*Copy (\S+) UP-Term:(\S+) ALL CLONE QUIET$', body)
                self.assertEqual(len(m), 1, '%s: one drawer copy' % name)
                want.append(('copy', m[0][0], m[0][1]))
            else:
                want.append(('step', name))
        got = []
        for m in re.finditer(r'\(set #step "([a-z-]+)"|\(set #from \(tackon #files "([^"]+)"\) #to \(tackon #dest "([^"]+)"\)', INSTALLER):
            got.append(('step', m.group(1)) if m.group(1) else ('copy', m.group(2), m.group(3)))
        self.assertEqual(got, want)
        # each #step / #from is followed by its run
        self.assertEqual(INSTALLER.count('(P_STEP)'), sum(1 for w in want if w[0] == 'step'))
        self.assertEqual(INSTALLER.count('(P_COPY)'), sum(1 for w in want if w[0] == 'copy'))

    def test_each_part_runs_alone_with_step(self):
        self.assertIn('If NOT "{STEP}" EQ ""\n  Skip {STEP}\nEndIf\nLab drawer\n', DOS)
        end = 'If NOT "{STEP}" EQ ""\n  Quit 0\nEndIf\n'
        for name, body in dos_parts()[:-1]:
            self.assertTrue(body.endswith(end), 'part %s does not end with the STEP quit' % name)
        for name, body in dos_parts():
            self.assertEqual(len(re.findall(r'(?m)^\s*If\b', body)), len(re.findall(r'(?m)^\s*EndIf\b', body)),
                             'part %s: If and EndIf do not pair' % name)

    def test_installer_passes_every_keyword(self):
        keys = re.match(r'\.KEY (\S+)', DOS).group(1).split(',')
        names = [k.split('/')[0] for k in keys if k.split('/')[0] not in ('DIR', 'STEP')]
        missing = [n for n in names if not re.search(r'"(UPTOPT)?%s[="\s]' % n, INSTALLER)]
        self.assertEqual(missing, [], 'install.dos keywords the Installer never passes')

    def test_options_travel_in_env_files_and_every_keyword_is_read_from_them(self):
        keys = re.match(r'\.KEY (\S+)', DOS).group(1).split(',')
        names = [k.split('/')[0] for k in keys if k.split('/')[0] not in ('DIR', 'STEP')]
        body = DOS[DOS.index('If NOT "{STEP}" EQ ""\n  If EXISTS ENV:UPTOPT'):DOS.index('; The parts, each')]
        for n in names:
            self.assertIn('  If EXISTS ENV:UPTOPT%s\n' % n, body, 'install.dos never reads UPTOPT%s' % n)
            self.assertEqual(DOS.count('{%s}' % n), 1, '{%s} is read once, into $o_%s' % (n, n.lower()))
        self.assertEqual(sorted(re.findall(r'\{(\w+)\}', DOS.replace('{STEP}', '').replace('{DIR}', ''))),
                         sorted(names), 'a {KEYWORD} used outside the prelude')
        opts = INSTALLER[INSTALLER.index('(procedure P_OPTIONS_LIST'):INSTALLER.index('(procedure P_OPTIONS_LIST') + 1200]
        opts = opts[:opts.index('(P_OPT))\n') + 9]
        for n in set(names) - {'CONSOLE', 'NOCONSOLE', 'DEVICE', 'NODEVICE'}:
            self.assertIn('"%s"' % n, opts, 'the Installer never writes UPTOPT%s' % n)
        self.assertIn('(set #name #con #val #con)', opts)
        self.assertIn('(set #name #dev #val #dev)', opts)
        self.assertLess(INSTALLER.index('\n(P_OPTIONS)\n'), INSTALLER.index('(set #step "drawer"'))
        self.assertLess(INSTALLER.index('\n(P_OPTIONS_END)\n'), INSTALLER.rindex('(P_END)'))
        self.assertRegex(INSTALLER, r'\(P_OPTIONS_END\)\n\s+\(abort', 'a failed part removes them too')


class TheBaseNamesNoAddOn(unittest.TestCase):
    """KIT-SPLIT: Neovim, Python, Claude and amiga-pi are archives of their own; the base's scripts, parts
    and keywords name none of the paths dist/packages.txt hands over, and the Installer has no page for them."""
    import mkmanifest
    HANDED = mkmanifest.handed_paths('base')

    def test_the_base_install_dos_never_copies_a_handed_path(self):
        self.assertTrue(self.HANDED)
        for path in self.HANDED:
            name = path.split('/', 1)[1]            # Files/nvim -> nvim
            self.assertNotRegex(DOS, r'(?m)^\s*(Copy|Protect)\b[^;\n]*\b%s\b' % re.escape(name), path)
            self.assertNotIn('copy-' + name.lower(), [n for n, _ in dos_parts()], path)

    def test_the_keywords_of_the_moved_parts_are_gone(self):
        keys = re.match(r'\.KEY (\S+)', DOS).group(1)
        for kw in ('PYTHON', 'NVIM', 'REMOTE'):
            self.assertNotIn(kw, keys)
            self.assertNotIn('opt' + kw.lower(), DOS)
            self.assertNotIn('"%s"' % kw, INSTALLER)

    def test_the_installer_has_no_python_neovim_or_server_page(self):
        for word in ('#python', '#nvim', '#remote', 'ENVARC:Claude', 'UPTermRemote'):
            self.assertNotIn(word, INSTALLER, word)

    def test_the_welcome_and_the_drawer_prompt_name_the_add_ons_as_archives(self):
        self.assertIn('Neovim, Python, Claude and amiga-pi are archives of', INSTALLER)
        self.assertIn('the files of the add-on archives', INSTALLER)

    def test_the_base_ships_upkg_conf_and_makes_the_opkg_drawers(self):
        """P10.5: etc/upkg.conf from dist/, the empty lists, info and cache drawers made by the Installer
        (not by lha), the vsh script upkg gone (UP-Term:bin/upkg is the opkg binary from the userland)"""
        mk = (ROOT / 'Makefile').read_text()
        self.assertIn('dist/upkg.conf', mk)
        self.assertNotIn('dist/upkg ', mk)
        self.assertNotIn('dist/upkg\n', mk)
        self.assertFalse((ROOT / 'dist/upkg').exists())
        self.assertIn('MakeDir >NIL: UP-Term:pkg UP-Term:pkg/upkg UP-Term:pkg/upkg/lists UP-Term:pkg/upkg/info UP-Term:pkg/upkg/cache', DOS)
        self.assertIn('Copy upkg.conf UP-Term:etc/upkg.conf CLONE', DOS)
        self.assertNotIn('Copy upkg UP-Term:bin/upkg', DOS)

    def test_a_package_install_prints_no_cant_set_uid_warning(self):
        """The A1200 2026-10-20: every entry of an add-on printed "Can't set UID=0" (opkg extracts with
        ARCHIVE_EXTRACT_OWNER; AmigaDOS has no owners). The host build of the same opkg, which cannot
        chown either, installs a one-file package with the shipped conf and must print no such line."""
        import subprocess, tempfile, tarfile, pkgs
        sys.path.insert(0, str(ROOT / 'tools/rig'))
        import paths
        upkg = paths.repo('upterm-ports') / 'build/host/opkg/inst/bin/upkg'
        if not upkg.is_file():
            self.skipTest('no host upkg: make -C ../upterm-ports host-opkg')
        ctl = b'Package: uidtest\nVersion: 1\nArchitecture: m68k-amigaos\nDescription: x\n'
        d = pkgs._tinfo('./', mode=0o755, kind=tarfile.DIRTYPE)
        ipk = pkgs._ar([('debian-binary', b'2.0\n'),
                        ('control.tar.gz', pkgs._targz([(d, None), (pkgs._tinfo('./control', len(ctl)), ctl)])),
                        ('data.tar.gz', pkgs._targz([(d, None), (pkgs._tinfo('./f', 2), b'x\n')]))])
        with tempfile.TemporaryDirectory() as t:
            t = pathlib.Path(t)
            for sub in ('lists', 'info', 'cache'):
                (t / 'root/pkg/upkg' / sub).mkdir(parents=True)
            conf = re.sub(r'(?m)^src/gz .*$', '', (ROOT / 'dist/upkg.conf').read_text())
            (t / 'c.conf').write_text(conf.replace('/UP-Term/pkg', str(t / 'root/pkg')))
            (t / 'u.ipk').write_bytes(ipk)
            p = subprocess.run([str(upkg), '-f', str(t / 'c.conf'), '-o', str(t / 'root'), 'install', str(t / 'u.ipk')],
                               capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
            self.assertTrue((t / 'root/f').is_file())
            self.assertNotIn('UID', p.stdout + p.stderr)

    def test_install_writes_the_upkg_alias_block_once_and_uninstall_strips_it(self):
        # the unix part runs in a base Install and in an Update (Install.installer: the part list)
        part = DOS[DOS.index('Lab unix'):DOS.index('Lab desktop')]
        self.assertIn('Search >NIL: S:Shell-Startup ";BEGIN UP-Term upkg"', part)
        self.assertIn('Echo >>S:Shell-Startup "Alias upkg UP-Term:bin/upkg []"', part)
        self.assertIn('Echo >>S:Shell-Startup ";END UP-Term upkg"', part)
        self.assertIn('If NOT EXISTS S:Shell-Startup', part)
        # the Search guard comes before the append, so a second run adds nothing
        self.assertLess(part.index('Search >NIL: S:Shell-Startup'), part.index('Echo >>S:Shell-Startup ";BEGIN'))
        self.assertIn('"unix"', mi.build(ROOT / "dist/Install.installer", "base"))
        un = mi.build(ROOT / "dist/Uninstall", "base")
        self.assertIn('"" S:Shell-Startup', un)

    def test_uninstall_leaves_the_add_ons_binaries_to_their_own_uninstall(self):
        un = mi.build(ROOT / "dist/Uninstall", "base")
        delete = [l for l in un.splitlines() if l.startswith('Delete >NIL: L:vtcon-handler')][0]
        for name in ('C:Claude', 'C:ClaudeCode', 'C:amiga-pi'):
            self.assertNotIn(name + ' ', delete + ' ')
        # the add-ons' own Uninstall runs first, from their records, then the single kit's leftovers
        self.assertLess(un.index('Execute UP-Term:pkg/up-nvim/Uninstall'), un.index('Delete >NIL: L:vtcon-handler'))
        self.assertLess(un.index('If EXISTS UP-Term:pkg/legacy'), un.index('Delete >NIL: L:vtcon-handler'))
        # only what needs the base is run from its record; the stand-alone ones (Claude, amiga-pi, Python3,
        # record in ENVARC:upkg) are left alone, and the single kit's leftovers go unless they took over
        self.assertNotIn('Execute ENVARC:upkg', un)
        self.assertNotIn('Execute UP-Term:pkg/UP-Term-Claude', un)
        legacy = un[un.index('If EXISTS UP-Term:pkg/legacy'):]
        self.assertIn('If NOT EXISTS ENVARC:upkg/up-claude', legacy.split('Set unst')[0])
        self.assertIn('If NOT EXISTS ENVARC:upkg/up-pi', legacy.split('Set unst')[0])

    def test_the_bases_uninstall_keeps_what_the_stand_alone_archives_share_with_it(self):
        """C:uptelnet is the base's and up-claude.lha's; Python3: is the base's old block's or up-python.lha's"""
        un = mi.build(ROOT / "dist/Uninstall", "base")
        self.assertNotRegex(un, r'Delete >NIL: [^\n]*C:uptelnet [^\n]*L:vtcon|Delete >NIL: L:vtcon-handler[^\n]*C:uptelnet')
        self.assertIn('If NOT EXISTS ENVARC:upkg/up-claude\n  Delete >NIL: C:uptelnet QUIET', un)
        self.assertIn('If NOT EXISTS ENVARC:upkg/up-python\n  Assign >NIL: Python3: EXISTS', un)

    def test_uninstall_asks_about_the_var_drawer_once_before_the_add_ons_run(self):
        un = mi.build(ROOT / "dist/Uninstall", "base")
        self.assertEqual(un.count('Ask "Also delete your data'), 1)
        self.assertLess(un.index('Ask "Also delete your data'), un.index('Execute UP-Term:pkg/'))
        self.assertLess(un.index('Set upvardelete NO'), un.index('Execute UP-Term:pkg/'))

    def test_update_over_the_single_kit_names_what_stays(self):
        self.assertIn('(getenv "UPTHANDED")', INSTALLER)
        for path in ('Files/nvim', 'Files/python3', 'Files/Claude', 'Files/amiga-pi'):
            self.assertIn('(patmatch "#?%s#?" #handed)' % path, INSTALLER)

    def test_the_base_record_keeps_a_single_kits_manifest_before_replacing_it(self):
        rec = dict(dos_parts())['record']
        self.assertLess(rec.index('Copy UP-Term:MANIFEST UP-Term:VERSIONS UP-Term:pkg/legacy'), rec.index('Copy MANIFEST UP-Term:MANIFEST'))
        for path in self.HANDED:
            if not any(q != path and path.startswith(q) for q in self.HANDED):
                self.assertIn('Search >NIL: UP-Term:MANIFEST " %s"' % path, rec)

    def test_the_readme_lists_every_add_on_archive_and_holds_none_of_their_manuals(self):
        readme = (ROOT / "dist/README.txt").read_text(encoding="latin-1")
        section = readme[readme.index("\nADD-ONS\n"):readme.index("\nSTATUS\n")]
        import pkgs
        for r in pkgs.lha_rows():
            if r['id'] != 'base':
                self.assertIn(r['archive'] + '.lha', section, r['id'])
        for heading in ("CLAUDE", "CLAUDE FROM THE AMIGA", "AMIGA-PI"):
            self.assertNotIn("\n" + heading + "\n", readme)
        for kw in ("PYTHON", "NVIM", "REMOTE"):
            self.assertNotIn("[" + kw, readme)

    def test_the_base_version_file_is_installed_by_the_tools_part(self):
        self.assertIn('Copy UP-Term.version UP-Term:UP-Term.version CLONE', dict(dos_parts())['tools'])
        self.assertRegex((ROOT / "dist/parts.txt").read_text(), r"(?m)^tools\s+Files/UP-Term.version$")


class FindsTheAddOns(unittest.TestCase):
    """The base Install finds the add-on archives beside its kit (plan 2026-10-18-installer-finds-addons):
    the list is dist/packages.txt's non-base rows, a drawer wins over an lha, every probe is (noreq)
    and each ticked add-on runs through a nested Installer."""
    import pkgs
    ADDONS = [r for r in pkgs.lha_rows() if r['id'] != 'base']

    def test_every_non_base_row_is_searched_offered_and_run_and_nothing_else(self):
        self.assertTrue(self.ADDONS)
        for n, r in enumerate(self.ADDONS):
            self.assertIn('(set #adarc "%s")\n(P_ADFIND)' % r['archive'], INSTALLER, r['id'])
            self.assertIn('(cat "%s" (if (= #ad%skind "drawer")' % (r['archive'], r['id']), INSTALLER, r['id'])
            self.assertIn('(if (and (in #adpick %d) (<> #ad%skind ""))' % (n, r['id']), INSTALLER, r['id'])
        self.assertEqual(INSTALLER.count('(P_ADFIND)'), len(self.ADDONS))
        self.assertIn('(default %d)' % (2 ** len(self.ADDONS) - 1), INSTALLER)
        self.assertNotIn('@ADDON', INSTALLER)
        # no archive name of a row is typed into the search/page/run code of the template
        template = (ROOT / "dist/Install.installer").read_text(encoding="latin-1")
        code = template[template.index('; ---- add-ons found beside'):template.index('(if (not #update)')]
        for r in self.ADDONS:
            self.assertNotIn(r['archive'], re.sub(r'(?m);.*$', '', code), r['id'])

    def test_a_drawer_is_tried_before_an_lha(self):
        body = INSTALLER[INSTALLER.index('(procedure P_ADFIND'):INSTALLER.index('(procedure P_ADRUN')]
        self.assertLess(body.index('(exists #adsrc (noreq))'), body.index('List >T:UPTADDLST'))
        self.assertIn('(if (exists #adsrc (noreq))\n        (set #adkind "drawer")', body)
        self.assertIn('PAT \\"" #adarc "(|-#?).lha\\"', body)

    def test_every_probe_of_the_add_on_search_has_noreq(self):
        start = INSTALLER.index('; ---- add-ons found beside')
        part = INSTALLER[start:INSTALLER.index('(if (not #update)', start)]
        probes = re.findall(r'\((?:exists|getsize|getversion)\b[^\n]*', part)
        self.assertTrue(probes)
        for p in probes:
            self.assertIn('(noreq)', p)

    def test_an_lha_is_unpacked_with_lha_before_its_install_runs_nested(self):
        body = INSTALLER[INSTALLER.index('(procedure P_ADRUN'):INSTALLER.index('(procedure P_ADDONS')]
        self.assertIn('lha -q x \\"" #adsrc "\\" \\"" #adhomep "\\""', body)
        self.assertLess(body.index('lha -q x'), body.index('Installer SCRIPT'))
        self.assertIn('Installer SCRIPT \\"" #adscript "\\" APPNAME \\"" #adarc\n'
                      '                                  "\\" MINUSER AVERAGE DEFUSER AVERAGE NOLOG"', body)
        self.assertIn('(if (not #adok) (set #adfail (cat #adfail ", " #adarc)))', body)

    def test_the_add_ons_run_after_the_install_and_before_the_last_page_of_a_full_install_and_an_update(self):
        self.assertEqual(INSTALLER.count('(P_ADDONS)'), 2)
        update = INSTALLER[INSTALLER.index('; ---- an Update ends here'):INSTALLER.index('; ---- what went where')]
        self.assertIn('(P_ADDONS)', update)
        self.assertIn('#adnote', update)
        full = INSTALLER[INSTALLER.index('; ---- what went where'):]
        self.assertLess(full.index('(P_OPTIONS_END)'), full.index('(P_ADDONS)'))
        self.assertLess(full.index('(P_ADDONS)'), full.rindex('(P_END)'))
        self.assertIn('#adnote))\n(if (= #wasabi "WASABI")', full)


class OffersAReboot(unittest.TestCase):
    """The handlers, XCON: and the assigns start with the next boot, so the
    last page of a full install and of an Update asks to reboot."""
    def setUp(self):
        a = INSTALLER.index('(procedure P_END')
        self.proc = INSTALLER[a:INSTALLER.index('\n\n', a)]

    def test_both_last_pages_end_in_p_end_and_nothing_else_exits(self):
        update = INSTALLER[INSTALLER.index('; ---- an Update ends here'):INSTALLER.index('; ---- what went where')]
        full = INSTALLER[INSTALLER.index('; ---- what went where'):]
        self.assertIn('(P_END)))', update)
        self.assertTrue(full.rstrip().endswith('(P_END)'))
        self.assertEqual(INSTALLER.count('(exit'), 1)
        self.assertIn('(exit (quiet))', self.proc)

    def test_the_question_says_reboot_or_later_and_later_is_the_default(self):
        self.assertIn('(message #done)', self.proc)
        self.assertIn('(askbool', self.proc)
        self.assertIn('(choices "Reboot" "Later")', self.proc)
        self.assertIn('(default 1)', self.proc)

    def test_only_a_yes_runs_the_kits_flushing_reboot(self):
        ask = self.proc[self.proc.index('(if (askbool'):]
        self.assertEqual(self.proc.count('(run '), 1)
        self.assertIn('(run (cat "\\"" (tackon #files "Reboot") "\\""))', ask)
        self.assertNotIn('(reboot', self.proc)

    def test_the_tools_part_installs_c_reboot_only_over_one_of_ours(self):
        self.assertIn('tools           Files/Reboot',
                      (ROOT / 'dist/parts.txt').read_text())
        self.assertIn('Search >NIL: C:Reboot "(UP-Term)"\n  If NOT WARN\n'
                      '    Copy Reboot C:Reboot CLONE', DOS)
        un = (ROOT / 'dist/Uninstall').read_text()
        self.assertIn('Search >NIL: C:Reboot "(UP-Term)"', un)
        self.assertIn('Delete >NIL: C:Reboot QUIET', un)
        self.assertIn('(UP-Term)"', (ROOT / 'install/reboot.c').read_text())


class AmiSSLPart(unittest.TestCase):
    """P7: the base kit ships AmiSSL 5.27 and the Install has a part that runs AmiSSL's OWN Install-AmiSSL
    (never a copy of its logic) when LIBS:amisslmaster.library is missing or older than the kit's."""
    PAGE = INSTALLER[INSTALLER.index('(set #amisslins'):INSTALLER.index('(set #net')] if '(set #amisslins' in INSTALLER else ''
    RUN = INSTALLER[INSTALLER.index('(procedure P_AMISSL'):INSTALLER.index('(procedure P_ADFIND')] if '(procedure P_AMISSL' in INSTALLER else ''

    def test_the_room_check_counts_amissl_on_the_disk_install_amissl_picks(self):
        s = INSTALLER
        # the assembled script: mkinstaller filled @KBAMISSL@ with kitsize's figure
        self.assertRegex(s, r'\(set #kb-amissl \(if #amisslins [1-9][0-9]+ 0\)\)')
        # Install-AmiSSL's own default: the AmiSSL: assign, else Work:, else SYS:
        self.assertIn('(getassign "AmiSSL")', s)
        self.assertIn('(getdiskspace #amissldisk)', s)
        self.assertLess(s.index('(getdiskspace #amissldisk)'), s.index('(P_AMISSL)\n'))
        self.assertNotIn('not counted in the room check', s)

    def test_amissl_on_the_drawers_volume_or_on_sys_is_added_to_that_volumes_need(self):
        # Work: holds both the drawer and AmiSSL by default: two checks that pass alone could overfill it
        s = INSTALLER
        self.assertIn('(set #needkb (+ #totalkb (if (= #vol #amisslvol) #kb-amissl 0)))', s)
        self.assertIn('(+ #kb-system #kb-rest (if (= #amisslvol "SYS:") #kb-amissl 0))', s)

    def test_the_part_exists_and_starts_install_amissl_from_the_kit_drawer(self):
        self.assertIn('AmiSSL 5.27', self.PAGE)
        self.assertIn('Install-AmiSSL', self.RUN)
        self.assertIn('Installer SCRIPT', self.RUN)
        self.assertIn('(tackon #files "AmiSSL")', self.RUN)
        self.assertIn('Installer SCRIPT Install-AmiSSL APPNAME AmiSSL', self.RUN)
        self.assertIn('(P_AMISSL)', INSTALLER)
        self.assertLess(INSTALLER.index('(P_OPTIONS_END)'), INSTALLER.index('(P_AMISSL)'))

    def test_its_default_follows_the_version_check_against_the_shipped_library(self):
        inc = (ROOT / "dist/common/amissl.installer").read_text(encoding="latin-1")
        self.assertIn('(getversion "LIBS:amisslmaster.library"', inc)
        self.assertIn('Libs/AmigaOS3/amisslmaster.library', inc)
        self.assertRegex(inc, r'\(set #amissl-need\s+\(<')
        self.assertIn('(default (if #amissl-need 1 0))', self.PAGE)

    def test_after_the_run_the_same_version_check_decides_installed_or_not(self):
        inc = (ROOT / "dist/common/amissl.installer").read_text(encoding="latin-1")
        self.assertIn('(procedure P_AMISSLCHECK', inc)
        self.assertEqual(inc.count('(getversion "LIBS:amisslmaster.library"'), 1)
        self.assertNotIn('getversion', (ROOT / "dist/Install.installer").read_text(encoding="latin-1"))
        self.assertEqual(INSTALLER.count('(P_AMISSLCHECK)'), 2)  # the include's call for the page's default, and the one after the run
        self.assertLess(self.RUN.index('(run "Execute T:UPTAMISSL")'), self.RUN.index('(P_AMISSLCHECK)'))

    def test_the_last_page_says_when_ticked_amissl_did_not_get_installed(self):
        run = self.RUN
        self.assertRegex(run, r'\(if #amissl-need\s+\(\s*\(set #sslnote \(cat "\\nAmiSSL was not installed')
        self.assertIn('Install-AmiSSL', run[run.index('AmiSSL was not installed'):][:200])
        self.assertIn('(set #sslrows 2)', run)

    def test_the_last_page_asks_for_a_reboot_when_amissl_was_installed(self):
        run = self.RUN
        self.assertRegex(run, r'\(set #sslnote "\\nReboot once: then LIBS: has AmiSSL\."\)')
        self.assertIn('(set #sslrows 1)', run)
        self.assertLess(run.index('AmiSSL was not installed'), run.index('Reboot once'))

    def test_the_notes_reach_the_last_page_and_its_line_count(self):
        s = INSTALLER
        tail = s[s.index('(P_ADDONS)\n'):]
        rows = '(set #adrows (+ #sslrows (if (<> #adnote "") 2 0)))'
        merge = '(set #adnote (cat #sslnote #adnote))'
        self.assertIn(rows, tail)
        self.assertLess(tail.index(rows), tail.index(merge))
        self.assertLess(tail.index(merge), tail.index('(set #n (+ (if (= #wasabi'))
        self.assertIn('(if (= #wasabi "WASABI") 14 9) #adrows)', tail)
        self.assertIn('(set #sslnote "")', s[s.index('(procedure P_AMISSL\n'):][:300])
        self.assertIn('(set #sslrows 0)', s[s.index('(procedure P_AMISSL\n'):][:300])

    def test_curl_names_the_amissl_it_uses(self):
        self.assertIn('"curl (AmiSSL 5 is installed first)"', INSTALLER)

    def test_uninstall_says_it_leaves_amissl_in_place(self):
        self.assertRegex((ROOT / "dist/Uninstall").read_text(encoding="latin-1"), r'(?i);[^\n]*AmiSSL[^\n]*(stays|left|shared)')

    def test_licences_carry_the_amissl_notice_and_the_drawer_is_a_kit_row(self):
        lic = (ROOT / "dist/LICENSES.txt").read_text(encoding="latin-1")
        self.assertIn("AmiSSL 5.27", lic)
        self.assertIn("Apache", lic[lic.index("AmiSSL 5.27"):][:400])
        self.assertRegex((ROOT / "dist/parts.txt").read_text(), r"(?m)^-\s+Files/AmiSSL$")
        mk = (ROOT / "Makefile").read_text()
        self.assertIn("vendor/amissl-5.27/AmiSSL", mk)


class WasabiDaemonOption(unittest.TestCase):
    """wasabid 0.11b2 is the ONE daemon option of the kit's Install (ticked like every option): it replaces the
    amiagent option and the wasabid 0.2 one. Install removes our old amiagent, stops and replaces our
    old wasabid, and Uninstall still takes both out. The binaries come from the build, not the repository."""
    PIN = (ROOT / "tools/nas/wasabi.rev").read_text().strip()

    def test_there_is_no_amiagent_option_any_more(self):
        self.assertNotIn("#agent", INSTALLER)
        self.assertNotIn("AMIAGENT", INSTALLER)
        self.assertNotIn("CPU000", INSTALLER)
        self.assertNotIn("AMIAGENT", DOS.splitlines()[0])
        self.assertNotIn("optamiagent", DOS)
        self.assertNotIn("Copy net/amiagent", DOS)
        self.assertFalse((ROOT / "dist/net/amiagent-0.13.2").exists())
        self.assertNotIn("AMIAGENT", (ROOT / "dist/README.txt").read_text(encoding="latin-1"))

    def test_the_boot_page_offers_wasabi_0_10b4_and_names_the_key(self):
        a = INSTALLER.index('(set #boot')
        page = INSTALLER[a:INSTALLER.index('(if (in #boot 0)', a)]
        self.assertIn('wasabi 0.11b2', page)
        self.assertIn('wasabi_mcp.py', page)
        self.assertIn('ENVARC:wasabi.key', page)
        self.assertIn('(if (in #boot 1) (set #wasabi "WASABI"))', INSTALLER)
        self.assertIn('(set #name "WASABI" #val #wasabi) (P_OPT)', mi.build(ROOT / "dist/Install.installer", "base"))

    def test_install_removes_our_old_amiagent_stopped_first_and_only_by_its_marker(self):
        blk = DOS[DOS.index('If EXISTS ENVARC:up-term/amiagent'):DOS.index('Lab fonts')]
        self.assertIn('Break $upw C', blk)
        self.assertLess(blk.index('Break $upw C'), blk.index('Delete >NIL: UP-Term:bin/amiagent'))
        self.assertIn('ENVARC:up-term/amiagent QUIET', blk)

    def test_install_replaces_an_old_wasabid_of_ours_but_keeps_a_0_9_when_unticked(self):
        blk = DOS[DOS.index('Set oldwasabid ""'):DOS.index('MakeDir >NIL: ENVARC:vsh')]
        self.assertIn('Search >NIL: C:wasabid "wasabid 0.11b2"', blk)        # the version marks the old one
        # a ticked install never stops a running wasabid: an Install run over wasabi died with it (the A1200, 2026-10-17)
        brk = blk[blk.index('If NOT "$optwasabi" EQ "WASABI"'):]
        self.assertEqual(blk.count('Break $upw C'), 1)
        self.assertLess(blk.index('If NOT "$optwasabi" EQ "WASABI"'), blk.index('Break $upw C'))
        self.assertLess(brk.index('Break $upw C'), brk.index('If "$optwasabi" EQ "WASABI"'))
        rm = blk[blk.index('If NOT "$optwasabi" EQ "WASABI"'):blk.index('UnSet >NIL: oldwasabid')]
        self.assertIn('If "$oldwasabid" EQ "1"', rm)                       # unticked removes only the old one
        self.assertIn('Delete >NIL: C:wasabid ENVARC:up-term/wasabid QUIET', rm)
        self.assertRegex(blk, r'(?im)^\s*Run >NIL: C:wasabid')            # a ticked install starts the new one

    def test_the_drawer_holds_licence_and_source_note_but_no_binary(self):
        d = ROOT / "dist/wasabi"
        self.assertEqual(sorted(p.name for p in d.iterdir()), ["LICENSE", "SOURCE.txt"])
        self.assertIn("MIT License", (d / "LICENSE").read_text())
        self.assertIn(self.PIN, (d / "SOURCE.txt").read_text())            # the pin of the build is the one named
        self.assertIn("wasabid 0.11b2", (d / "SOURCE.txt").read_text())
        lic = (ROOT / "dist/LICENSES.txt").read_text(encoding="latin-1")
        self.assertIn("wasabi 0.11b2", lic)
        self.assertIn("MIT", lic[lic.index("wasabi 0.11b2"):][:200])
        self.assertNotIn("amiagent", lic)
        self.assertIn("wasabi_mcp.py", (ROOT / "dist/README.txt").read_text(encoding="latin-1"))
        mk = (ROOT / "Makefile").read_text()
        self.assertIn("cp $(BUILD)/wasabi/wasabid $(BUILD)/wasabi/wasabi $(BUILD)/wasabi/wasabi_mcp.py $(BUILD)/wasabi/wasabi_client.py", mk)
        self.assertNotIn("dist/net/amiagent", mk)
        self.assertRegex((ROOT / "dist/parts.txt").read_text(), r"(?m)^boot\s+Files/wasabi$")

    def test_uninstall_never_stops_a_running_wasabid(self):
        # an Uninstall run over wasabi died with the wasabid it stopped (the A1200, 2026-10-17)
        un = mi.build(ROOT / "dist/Uninstall", "base")
        self.assertNotIn("COM C:wasabid", un)
        mark = un.index("If EXISTS ENVARC:up-term/wasabid")
        self.assertLess(mark, un.index("Delete >NIL: C:wasabid QUIET"))
        self.assertLess(un.index("Delete >NIL: C:wasabid QUIET"), un.index("EndIf", mark))

    def test_uninstall_keeps_a_boot_line_for_a_wasabid_that_was_there_before_install(self):
        # UNINSTALL-WASABI-BLOCK: Install writes its wasabi block whenever S:User-Startup has no C:wasabid
        # line, also for a wasabid the owner put in C: (no marker). Stripping the block alone left that
        # wasabid with no boot line; a plain one takes the block's place (the owner, 2026-10-25).
        un = mi.build(ROOT / "dist/Uninstall", "base")
        decide = un.index('Set unwasabi ""')
        strip = un.index('$unvsh "$unst"\n')
        self.assertLess(decide, strip)
        cond = un[decide:strip]
        for need in ("If NOT EXISTS ENVARC:up-term/wasabid", "If EXISTS C:wasabid",
                     'Search >NIL: S:User-Startup ";BEGIN UP-Term wasabi"', "Set unwasabi 1"):
            self.assertIn(need, cond)
        copy = un.index("Copy T:User-Startup.up-term S:User-Startup CLONE QUIET")
        add = un.index('Echo >>S:User-Startup "Run >NIL: C:wasabid"')
        self.assertLess(copy, add)
        self.assertIn('If "$unwasabi" EQ "1"', un[copy:add])
        self.assertIn("UnSet unwasabi", un)

    def test_uninstall_stops_and_removes_both_daemons_before_the_drawer_goes(self):
        un = mi.build(ROOT / "dist/Uninstall", "base")
        stop = un.index("If EXISTS ENVARC:up-term/amiagent")
        self.assertIn("Break $upw C", un[stop:stop + 400])
        self.assertLess(stop, un.index('Delete >NIL: "$updest" ALL QUIET'))
        self.assertLess(stop, un.index("Delete >NIL: ENVARC:up-term ALL QUIET"))
        self.assertIn("Delete >NIL: C:wasabid QUIET", un[un.index("If EXISTS ENVARC:up-term/wasabid"):][:300])


class AmigaDosVariableNames(unittest.TestCase):
    """AmigaDOS ends a $name at the first character that is not a letter or a
    digit: $o_dest was the variable "o" (none), and the literal "$o_dest" went
    into a Replay's S:User-Startup, whose boot then failed (2026-10-07)."""
    SCRIPTS = ("install.dos", "Uninstall")

    def test_no_variable_name_in_the_kits_amigados_scripts_has_other_characters(self):
        for name in self.SCRIPTS:
            text = mi.build(ROOT / "dist" / name, "base")
            body = "\n".join(l for l in text.splitlines() if not l.lstrip().startswith(";"))
            self.assertEqual(re.findall(r'\$[A-Za-z0-9]+[^A-Za-z0-9\s"*/:;.,()<>{}\[\]=-]', body), [], name)
            self.assertEqual(re.findall(r'(?im)^\s*(?:Set|UnSet)(?:\s+[<>]\S*)*\s+(?![<>])([A-Za-z0-9]*[^A-Za-z0-9\s][^\s]*)', body),
                             [], name)

    def test_the_installer_writes_the_option_files_install_dos_reads(self):
        self.assertIn('(cat "ENV:UPTOPT" #name)', INSTALLER)
        self.assertIn('Set optdest "$UPTOPTDEST"', DOS)


class OneClaudeApp(unittest.TestCase):
    """CL1: Claude and ClaudeCode are one app. The ClaudeCode script is gone; C:Claude asks
    Amiga-or-NAS itself (claude/launch.c, tests ONLY=claude_cli / claude_tui). REMOTE-ADDRESS stays:
    the Installer's server page reads the old file's host and port through it."""

    def test_the_script_is_gone_and_the_installer_still_reads_the_remote_file_through_claude(self):
        self.assertFalse((ROOT / "dist/ClaudeCode").exists())
        page = (ROOT / "dist/addons/claude/Install.installer").read_text(encoding="latin-1")
        self.assertIn('REMOTE-ADDRESS >ENV:UPTermRemote', page)
        self.assertNotIn('C:ClaudeCode and', page)
        self.assertNotIn('ClaudeCode', page.replace('(an old C:ClaudeCode is removed)', '').replace('(the old ClaudeCode icon goes)', ''))


def step_command_line(files, dest, installer=INSTALLER):
    """The AmigaDOS line P_STEP's (execute ...) makes, worst case: every option on.
    Each argument of the form is evaluated: strings, (cat ...) and #variables."""
    toks = tokens(installer)
    # the form starts after "(set #rc" of P_STEP: the first (execute in the procedure
    p = next(k for k in range(len(toks) - 2) if toks[k] == ('word', 'procedure') and toks[k + 1] == ('word', 'P_STEP'))
    i = next(k for k in range(p, len(toks)) if toks[k] == ('word', 'execute')) - 1
    depth, j, args = 0, i, []
    cur = None
    while True:
        kind, text = toks[j]
        if kind == 'open':
            depth += 1
            if depth == 2:
                cur = []
        elif kind == 'close':
            depth -= 1
            if depth == 1:
                args.append(('cat', cur)); cur = None
            if depth == 0:
                break
        elif depth >= 1 and not (depth == 1 and text == 'execute'):
            (cur if depth == 2 else args).append((kind, text))
        j += 1
    kw = {'con': 'NOCONSOLE', 'dev': 'NODEVICE', 'bget': 'BEBBOGET',
          'files': files, 'script': files + '/install.dos', 'dest': dest, 'step': 'copy-coreutils'}
    def val(tok):
        kind, text = tok
        if kind == 'str':
            return text.replace('\\"', '"')
        name = text.lstrip('#')
        return kw.get(name, name.upper())
    out = []
    for a in args:
        if a[0] == 'cat':
            cat = a[1]
            if cat and cat[0] == ('word', 'cat'):
                cat = cat[1:]
            out.append(''.join(val(x) for x in cat))
        else:
            out.append(val(a))
    return 'Execute ' + ' '.join(out)


class StepCommandLine(unittest.TestCase):
    def test_worst_case_step_line_fits_the_255_character_limit(self):
        """INSTALLER-CMDLINE-TOO-LONG: a kit in a long path, every option on, a long
        DEST: the line P_STEP hands the Shell stays under AmigaDOS's limit."""
        files = 'Work:' + 'k' * 49 + '/Files'          # 60 characters
        self.assertEqual(len(files), 60)
        line = step_command_line(files, 'D' * 40)
        self.assertLess(len(line), 255, '%d characters: %s' % (len(line), line))


class EveryOptionTicked(unittest.TestCase):
    """ALL-TICKED (user, 2026-10-20): every checkbox of the Installer is ticked by default; the
    user unticks what they do not want. Each askoptions page's default is all its choices, except
    AmiSSL: the same version or a newer one is already installed, so it is ticked only when needed
    (AmiSSLPart checks that default)."""
    PAGES = {'#net': 3, '#extra': 3, '#boot': 2, '#sys': 2}

    def test_each_page_defaults_to_all_its_choices(self):
        for var, n in self.PAGES.items():
            a = INSTALLER.index('(set %s\n    (askoptions' % var)
            page = INSTALLER[a:INSTALLER.index('(default', a) + 40]
            d = page[page.index('(default') + 9:].split(')')[0]
            value = int(d) if d.isdigit() else sum(int(x) for x in d.lstrip('(+ ').split())
            self.assertEqual(value, 2 ** n - 1, var)
        # AmiSSL's and the add-on page (FindsTheAddOns: all add-ons) are the two more
        self.assertEqual(INSTALLER.count('(askoptions'), len(self.PAGES) + 2, 'a page this test misses')


class UpdateWithNoRecordedOptions(unittest.TestCase):
    """UPDATE-DETECT (owner, 2026-10-21): the 1.6 Install over a 1.5 install on a Replay offered no
    Update and said nothing: that install had left no ENVARC:up-term/opts (record.dos keeps them
    only when the Installer ran it). The base's Install now reads each option back from the Amiga
    (dist/common/oldopts.installer) into T:UPTOLDOPTS, the gate takes those, and the Update branch
    copies them to ENV:; every other reason an installed package gets no Update is said on a page
    before the full install."""

    def recorded(self):
        """The options P_OPTIONS_LIST writes: CONSOLE and DEVICE come as #con and #dev."""
        lst = INSTALLER[INSTALLER.index('(procedure P_OPTIONS_LIST'):]
        lst = lst[:lst.index('\n\n')]
        names = set(re.findall(r'\(set #name "([A-Z0-9]+)"', lst))
        self.assertIn('(set #name #con', lst)
        self.assertIn('(set #name #dev', lst)
        return names | {'CONSOLE', 'NOCONSOLE', 'DEVICE', 'NODEVICE'}

    def test_every_recorded_option_is_read_back(self):
        frag = INSTALLER[INSTALLER.index('(procedure P_OLDOPT'):INSTALLER.index('(set #oldopts 1)')]
        read = set(re.findall(r'#name (?:\(if #on )?"([A-Z0-9]+)"(?: "([A-Z0-9]+)")?', frag))
        read = {n for pair in read for n in pair if n}
        if 'T:UPTOLDOPTS/UPTOPTDEST' in frag:
            read.add('DEST')
        self.assertEqual(self.recorded() - read, set(), 'an option the Update would lose')

    def test_gate_and_restore_take_the_read_back_options(self):
        self.assertIn('(or #legacy #oldopts (exists "ENVARC:up-term/opts/UPTOPTDEST"))', INSTALLER)
        self.assertIn('(if #oldopts "Copy T:UPTOLDOPTS/#? ENV: QUIET" "Copy ENVARC:up-term/opts/#? ENV: QUIET")',
                      INSTALLER)
        # read back only when there is no record: a recorded install keeps its own choices
        frag = INSTALLER[INSTALLER.index('(procedure P_OLDOPT'):INSTALLER.index('(set #oldopts 1)')]
        self.assertIn('(not (exists "ENVARC:up-term/opts/UPTOPTDEST"))', frag)

    def test_the_drawer_read_back_is_the_one_the_install_named(self):
        # getassign gives the volume's name (System:UP-Term for SYS:UP-Term): a rig run read back
        # a DEST that differed from the full install's. install.dos keeps the drawer as named in
        # ENVARC:up-term/Dir, which ENV: holds from boot.
        frag = INSTALLER[INSTALLER.index('(procedure P_OLDOPT'):INSTALLER.index('(set #oldopts 1)')]
        self.assertIn('(set #olddir (getenv "up-term/Dir"))', frag)
        self.assertIn('(textfile (dest "T:UPTOLDOPTS/UPTOPTDEST") (append #olddir))', frag)
        self.assertNotIn('(append #olddrawer)', frag)
        self.assertIn('Echo >ENVARC:up-term/Dir "$updest"', (ROOT / 'dist/install.dos').read_text())

    def test_add_ons_do_not_read_the_base_options(self):
        import pkgs
        for row in pkgs.rows():
            if row['id'] == 'base' or not (ROOT / 'dist/addons' / row['id'] / 'Install.installer').exists():
                continue
            script = mi.build(ROOT / 'dist/addons' / row['id'] / 'Install.installer', row['id'])
            self.assertNotIn('UPTOLDOPTS', script, row['id'])
            self.assertIn('(set #oldopts 0)', script, row['id'])

    def test_says_why_there_is_no_update(self):
        self.assertIn('(if (<> #whyno "")\n    (message', INSTALLER)
        for why in ('kept no record of its choices', 'has no upupdate', 'could not compare',
                    'kept no list of the files'):
            self.assertIn(why, INSTALLER)
        # the page comes before the welcome, which a full install shows first
        self.assertLess(INSTALLER.index('(if (<> #whyno "")'), INSTALLER.index('(if (not #update)'))


class ShellIconStartsVsh(unittest.TestCase):
    """SHELL-ICON-VSH (user, 2026-10-20): the Shell icon option makes SYS:System/Shell start vsh
    in an UP-Term (XCON:) window; scripts and NewShell keep the AmigaDOS Shell. The icon as it was
    is saved first and Uninstall puts it back."""
    UPICON = (ROOT / "install/upicon.c").read_text()
    UNINSTALL = (ROOT / "dist/Uninstall").read_text()

    def test_the_option_saves_the_icon_then_points_it_at_vsh_in_xcon(self):
        a = DOS.index('If "$optshellicon" EQ "SHELLICON"')
        block = DOS[a:DOS.index('EndIf\nEndIf', a)]
        save = block.index('Copy SYS:System/Shell.info ENVARC:up-term-orig/Shell.info CLONE')
        run = block.index('upicon SYS:System/Shell WINDOWDEV=XCON TOOL=UP-Term:bin/vsh')
        self.assertLess(save, run)

    def test_upicon_takes_tool_and_sets_the_default_tool_of_a_project(self):
        self.assertIn('"NAME/A,WINDOWDEV/K/A,TOOL/K"', self.UPICON)
        self.assertIn('d->do_DefaultTool = (STRPTR)args[2];', self.UPICON)
        self.assertIn('d->do_Type = WBPROJECT;', self.UPICON)

    def test_nothing_else_points_the_amigados_shell_at_vsh(self):
        self.assertEqual(DOS.count('TOOL=UP-Term:bin/vsh'), 1)
        self.assertEqual([l.split()[1] for l in DOS.splitlines() if l.lstrip().startswith('upicon ')],
                         ['SYS:System/Shell'])

    def test_uninstall_puts_the_saved_icon_back(self):
        self.assertIn('Copy ENVARC:up-term-orig/Shell.info SYS:System/Shell.info CLONE', self.UNINSTALL)


class ConsoleTakeover(unittest.TestCase):
    """CON-TAKEOVER: UPConsole refuses CON ON while another console replacement serves CON:
    (DD21, never stack; the A1200's KingCON, 2026-10-20), and the Installer used to stop there.
    Now the refusal turns that replacement's Mount lines in S:User-Startup off (takecon.sh,
    tested in tests/test_sh_exec.c) and writes the boot block, so UP-Term serves CON: from the
    next boot; ENV:UPTCONREBOOT tells the last page, and the kit ships takecon.sh."""

    @staticmethod
    def con_branch():
        lines = [l.strip() for l in DOS.splitlines()]
        start = lines.index('C:UPConsole >T:UPTCON CON ON')
        return lines[start:lines.index('Delete >NIL: T:UPTCON QUIET', start)]

    def test_refusal_turns_the_other_console_off_and_writes_the_block(self):
        b = self.con_branch()
        warn = b.index('If WARN')
        run = b.index('vsh takecon.sh')
        found = b.index('If EXISTS T:UPTCONOFF')
        copy = b.index('Copy T:User-Startup.up-term S:User-Startup CLONE QUIET')
        flag = b.index('Copy T:UPTCONOFF ENV:UPTCONREBOOT QUIET')
        again = b.index('Set upcon "CONSOLE"')
        refuse = b.index('Copy T:UPTCON ENV:UPTREFUSEDCON QUIET')
        block = b.index('Echo >>S:User-Startup "C:UPConsole >NIL: CON ON"')
        self.assertTrue(warn < run < found < copy < flag < again < refuse < block, b)
        self.assertIn('ENV:UPTCONREBOOT', [w for l in DOS.splitlines() if l.startswith('Delete ')
                                            for w in l.split()])

    def test_last_page_reads_and_clears_the_reboot_note(self):
        self.assertIn('(exists "ENV:UPTCONREBOOT")', INSTALLER)
        self.assertIn('(run "Delete >NIL: ENV:UPTCONREBOOT QUIET")', INSTALLER)

    def test_kit_ships_takecon(self):
        self.assertIn('Files/takecon.sh', (ROOT / "dist/parts.txt").read_text().split())
        self.assertRegex((ROOT / "Makefile").read_text(), r'cp [^\n]*dist/takecon\.sh[^\n]*\$\(KIT\)/Files/')


class UserStartupBlocksAreGuarded(unittest.TestCase):
    """BOOT-UPTERM-VOLUME-LOOP: with the UP-Term drawer gone, an unguarded S:User-Startup line that
    names UP-Term: put up "Please insert volume UP-Term" at boot and nothing after it ran (wasabid
    too). Every such line now sits inside Assign >NIL: UP-Term: EXISTS / If NOT WARN, the assign
    inside If EXISTS "<drawer>"; an older unguarded block is rewritten; markers are unchanged."""

    def blocks(self):
        out, cur = {}, None
        for m in re.finditer(r'(?m)^\s*Echo >>S:User-Startup "(.*)"$', DOS):
            l = m.group(1).replace('*"', '"')
            if l.startswith(';BEGIN'):
                cur = out.setdefault(l, [])
            elif l.startswith(';END'):
                cur = None
            elif cur is not None:
                cur.append(l)
        return out

    def test_every_line_naming_upterm_is_guarded(self):
        seen = 0
        for name, lines in self.blocks().items():
            for i, l in enumerate(lines):
                if 'UP-Term:' not in l or l.strip() == 'Assign >NIL: UP-Term: EXISTS':
                    continue
                seen += 1
                if name == ';BEGIN UP-Term assign':
                    self.assertTrue(lines[i - 1].startswith('If EXISTS "'), (name, lines))
                    continue
                g = [j for j, x in enumerate(lines[:i]) if x.strip() == 'Assign >NIL: UP-Term: EXISTS']
                self.assertTrue(g, (name, lines))
                self.assertIn('If NOT WARN', [x.strip() for x in lines[g[-1] + 1:i]], (name, lines))
                self.assertEqual(lines[-1], 'EndIf', (name, lines))
        # the assign line, Amarchy, Amarchy index, GG
        self.assertEqual(seen, 4)

    def test_old_unguarded_block_is_replaced_on_update(self):
        for key in ('  Run >NIL: UP-Term:bin/Amarchy', '  Run >NIL: UP-Term:bin/amarchy-index',
                    '    Assign GG: UP-Term:', '  Assign UP-Term: *"$updest*"'):
            self.assertIn('Search >NIL: S:User-Startup "%s' % key, DOS)
        for call in ('vsh unstartup.sh Amarchy\n', 'vsh unstartup.sh "Amarchy index"\n',
                     'vsh unstartup.sh UP-Term\n', 'vsh reassign.sh "$updest"'):
            self.assertIn(call, DOS)

    def sh(self, script, args, tmp):
        import subprocess
        s = (ROOT / 'dist' / script).read_text().replace('S:', 'S_').replace('T:', 'T_').replace('ENV:', 'E_')
        r = subprocess.run(['sh', '-c', s, 'x', *args], cwd=tmp, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return pathlib.Path(tmp, 'T_User-Startup.up-term').read_text()

    OLD = ('; user\n;BEGIN UP-Term assign\nAssign UP-Term: "Work:UP-Term"\n;END UP-Term assign\n'
           ';BEGIN UP-Term Amarchy\nRun >NIL: UP-Term:bin/Amarchy\n;END UP-Term Amarchy\n'
           ';BEGIN UP-Term\nAssign >NIL: GG: EXISTS\nIf WARN\n  Assign GG: UP-Term:\nEndIf\n;END UP-Term\n'
           ';BEGIN UP-Term wasabi\nRun >NIL: C:wasabid\n;END UP-Term wasabi\nEcho keep\n')

    def test_reassign_rewrites_an_old_block_guarded_in_place(self):
        import tempfile
        with tempfile.TemporaryDirectory() as t:
            pathlib.Path(t, 'S_User-Startup').write_text(self.OLD)
            out = self.sh('reassign.sh', ['Work:UP-Term'], t)
            guarded = 'If EXISTS "Work:UP-Term"\n  Assign UP-Term: "Work:UP-Term"\nEndIf'
            self.assertIn(';BEGIN UP-Term assign\n' + guarded + '\n;END UP-Term assign\n', out)
            self.assertEqual(out.replace(guarded, 'Assign UP-Term: "Work:UP-Term"'), self.OLD)
            self.assertFalse(pathlib.Path(t, 'E_upold').exists())
            # a guarded block for another drawer moves to the new one
            pathlib.Path(t, 'S_User-Startup').write_text(out)
            out2 = self.sh('reassign.sh', ['Sys:Other'], t)
            self.assertIn('If EXISTS "Sys:Other"\n  Assign UP-Term: "Sys:Other"\nEndIf\n', out2)
            self.assertEqual(pathlib.Path(t, 'E_upold').read_text(), 'Work:UP-Term')

    def test_unstartup_strips_every_guarded_block(self):
        import tempfile
        text = '; user\n'
        for name, lines in self.blocks().items():
            text += '\n'.join([name] + lines + [name.replace('BEGIN', 'END')]) + '\n'
        text += 'Echo keep\n'
        self.assertIn('  Run >NIL: UP-Term:bin/Amarchy', text)
        with tempfile.TemporaryDirectory() as t:
            pathlib.Path(t, 'S_User-Startup').write_text(text)
            self.assertEqual(self.sh('unstartup.sh', [], t), '; user\nEcho keep\n')
            one = self.sh('unstartup.sh', ['Amarchy'], t)
            self.assertNotIn('bin/Amarchy', one)
            self.assertIn('bin/amarchy-index', one)
            gg = self.sh('unstartup.sh', ['UP-Term'], t)
            self.assertNotIn('Assign GG:', gg)
            self.assertIn('bin/Amarchy', gg)


class LinkedEnvOn32(unittest.TestCase):
    """UNINSTALL-HANGS-ON-32 / ENV-DRAWER-RESET: AmigaOS 3.2 links RAM:ENV to ENVARC:, so a
    drawer made in ENVARC: shows in ENV: at once. Deleting a drawer in that ENV: resets the
    machine (ram-handler 47.57, the A1200 Installer, 2026-10-20: Delete ENV:vsh once empty, and
    Delete ENV:up-term/#? ALL after a Copy ALL into it); deleting its files is safe. So no script
    deletes an ENV: drawer of the kit: each ENV: drawer Install fills is first emptied of its
    files (common/env-empty.dos), and Uninstall empties ENV: of files, then removes ENVARC:."""

    @staticmethod
    def env_drawer_deletes(script):
        out = []
        for l in script.splitlines():
            l = l.strip()
            if l.startswith(';') or not re.match(r'(?i)delete\b', l):
                continue
            for name in ('up-term', 'vsh'):
                # ENV:x itself, or anything under it with ALL (which takes its subdrawers)
                if re.search(r'(?i)(?<![\w])env:%s(?=[\s"]|$)' % name, l) or \
                   (re.search(r'(?i)(?<![\w])env:%s/' % name, l) and re.search(r'(?i)\ball\b', l)):
                    out.append(l)
        return out

    def test_no_script_deletes_an_env_drawer(self):
        for what, script in (('install.dos', DOS), ('Uninstall', mi.build(ROOT / "dist/Uninstall", "base"))):
            self.assertEqual(self.env_drawer_deletes(script), [], what)

    def test_every_env_drawer_install_fills_is_emptied_first(self):
        lines = [l.strip() for l in DOS.splitlines() if not l.strip().startswith(';')]
        filled = set()
        for i, l in enumerate(lines):
            m = (re.match(r'(?i)copy\s+\S+\s+env:([\w.-]+)\s.*\ball\b', l) or
                 re.match(r'(?i)makedir\s+(?:>nil:\s+)?env:([\w.-]+)\s*$', l))
            # the drawers Uninstall empties (ENV:Claude is the user's and stays)
            if not m or m.group(1).lower() in filled or m.group(1).lower() not in ('up-term', 'vsh'):
                continue
            name = m.group(1).lower()
            before = '\n'.join(lines[:i])
            set_at = before.lower().rfind('set upempty env:%s' % name)
            self.assertGreaterEqual(set_at, 0, 'install.dos fills ENV:%s without emptying it first: %s' % (name, l))
            self.assertIn('List >T:upempty "$upempty" ALL FILES', before[set_at:])
            filled.add(name)
        self.assertEqual(filled, {'up-term', 'vsh'})

    def test_uninstall_empties_env_then_removes_envarc(self):
        un = mi.build(ROOT / "dist/Uninstall", "base").lower()
        for name in ('up-term', 'vsh'):
            env = un.index('set upempty env:%s\n' % name)
            arc = un.index('set upempty envarc:%s\n' % name)
            gone = un.index('delete >nil: envarc:%s all quiet' % name)
            self.assertLess(env, arc, name)
            self.assertLess(arc, gone, name)


class TermOnlyInUpTermWindows(unittest.TestCase):
    """Y2-TERM: TERM=vtcon is vsh's to give, per window (it asks the console). A global TERM
    that says vtcon -- an earlier kit's README had the user SetEnv it -- makes a ROM CON: window
    get xterm sequences it cannot show (python3's REPL hangs on Up there, ledger Y1). Install and
    Uninstall delete such a TERM from ENV: and ENVARC:, and only when it says vtcon."""

    def guarded_delete(self, script, where):
        lines = [l.strip() for l in script.splitlines() if not l.strip().startswith(';')]
        for i, l in enumerate(lines):
            if re.fullmatch(r'(?i)delete\s+>nil:\s+%s:TERM\s+quiet' % where, l):
                return (re.fullmatch(r'(?i)search\s+>nil:\s+%s:TERM\s+vtcon' % where, lines[i - 2]) is not None
                        and lines[i - 1].lower() == 'if not warn')
        return False

    def test_install_removes_an_old_global_term_vtcon(self):
        for where in ('ENV', 'ENVARC'):
            self.assertTrue(self.guarded_delete(DOS, where), 'install.dos: no guarded delete of %s:TERM' % where)

    def test_uninstall_removes_an_old_global_term_vtcon(self):
        un = mi.build(ROOT / "dist/Uninstall", "base")
        for where in ('ENV', 'ENVARC'):
            self.assertTrue(self.guarded_delete(un, where), 'Uninstall: no guarded delete of %s:TERM' % where)

    def test_the_kit_never_sets_a_global_term(self):
        for name in ('dist/install.dos', 'dist/Install.installer', 'dist/up-term.conf', 'dist/vshrc'):
            for l in (ROOT / name).read_text().splitlines():
                self.assertFalse(re.match(r'(?i)\s*(c:)?setenv\s+(>nil:\s+)?term\s', l), '%s sets TERM globally: %s' % (name, l))
        readme = (ROOT / "dist/README.txt").read_text()
        self.assertNotRegex(readme, r'(?im)^\s*SetEnv TERM ')


class SoftFloatIxemul(unittest.TestCase):
    """A machine without an FPU gets the soft-float ixemul.library (Files/libs/ixemul.library.nofpu)."""
    MAKE = (ROOT / 'Makefile').read_text()
    PY = mi.build(ROOT / 'dist/addons/python/install.dos', 'python')

    def test_the_kits_carry_the_soft_float_build_beside_the_68881_one(self):
        self.assertRegex(self.MAKE, r'(?m)^IXEMUL_LIB_SOFT \?= .*soft-float/amigaos/ixemul\.library$')
        for kit in ('$(KIT)', '$(KIT_python)'):
            self.assertIn('cp $(IXEMUL_LIB_SOFT) %s/Files/libs/ixemul.library.nofpu\n' % kit, self.MAKE)

    def test_the_installer_hands_nofpu_down_and_the_base_script_picks_the_file_by_it(self):
        self.assertIn('(set #nofpu (if (= (database "fpu") "NOFPU") "NOFPU" ""))', INSTALLER)
        self.assertIn('(set #name "NOFPU" #val #nofpu) (P_OPT)', INSTALLER)
        self.assertIn('(set #nofpu (getenv "UPTOPTNOFPU"))', INSTALLER)      # an Update keeps the choice
        libs = DOS[DOS.index('Lab libs'):]
        libs = libs[:libs.index('Avail >NIL: FLUSH')]
        pick = libs.index('If "$optnofpu" EQ "NOFPU"')
        self.assertIn('Set upixlib libs/ixemul.library.nofpu', libs[pick:pick + 120])
        self.assertNotIn('Copy libs/ixemul.library', libs)                   # every copy goes through $upixlib
        self.assertEqual(libs.count('Copy $upixlib '), 2)
        self.assertLess(pick, libs.index('Copy $upixlib '))

    def test_the_python_add_on_picks_the_soft_float_file_on_nofpu(self):
        ix = self.PY[self.PY.index('Lab ixemul'):self.PY.index('Lab copy-python')]
        self.assertIn('If EXISTS ENV:UPTOPTNOFPU\n    Set ixlib libs/ixemul.library.nofpu', ix)
        self.assertLess(ix.index('Set ixlib libs/ixemul.library.nofpu'), ix.index('Copy $ixlib LIBS:ixemul.library'))
        head = (ROOT / 'dist/common/addon-head.installer').read_text()
        self.assertIn('(set #name "NOFPU" #val #nofpu) (P_OPT)', head)


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
