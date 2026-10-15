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
        self.assertLess(INSTALLER.index('\n(P_OPTIONS_END)\n'), INSTALLER.index('(exit #done)'))
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

    def test_uninstall_leaves_the_add_ons_binaries_to_their_own_uninstall(self):
        un = mi.build(ROOT / "dist/Uninstall", "base")
        delete = [l for l in un.splitlines() if l.startswith('Delete >NIL: L:vtcon-handler')][0]
        for name in ('C:Claude', 'C:ClaudeCode', 'C:amiga-pi'):
            self.assertNotIn(name + ' ', delete + ' ')
        # the add-ons' own Uninstall runs first, from their records, then the single kit's leftovers
        self.assertLess(un.index('Execute UP-Term:pkg/UP-Term-nvim/Uninstall'), un.index('Delete >NIL: L:vtcon-handler'))
        self.assertLess(un.index('If EXISTS UP-Term:pkg/legacy'), un.index('Delete >NIL: L:vtcon-handler'))
        # only what needs the base is run from its record; the stand-alone ones (Claude, amiga-pi, Python3,
        # record in ENVARC:uppkg) are left alone, and the single kit's leftovers go unless they took over
        self.assertNotIn('Execute ENVARC:uppkg', un)
        self.assertNotIn('Execute UP-Term:pkg/UP-Term-Claude', un)
        legacy = un[un.index('If EXISTS UP-Term:pkg/legacy'):]
        self.assertIn('If NOT EXISTS ENVARC:uppkg/Claude', legacy.split('Set unst')[0])
        self.assertIn('If NOT EXISTS ENVARC:uppkg/amiga-pi', legacy.split('Set unst')[0])

    def test_the_bases_uninstall_keeps_what_the_stand_alone_archives_share_with_it(self):
        """C:uptelnet is the base's and Claude.lha's; Python3: is the base's old block's or Python3.lha's"""
        un = mi.build(ROOT / "dist/Uninstall", "base")
        self.assertNotRegex(un, r'Delete >NIL: [^\n]*C:uptelnet [^\n]*L:vtcon|Delete >NIL: L:vtcon-handler[^\n]*C:uptelnet')
        self.assertIn('If NOT EXISTS ENVARC:uppkg/Claude\n  Delete >NIL: C:uptelnet QUIET', un)
        self.assertIn('If NOT EXISTS ENVARC:uppkg/Python3\n  Assign >NIL: Python3: EXISTS', un)

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
        for r in pkgs.rows():
            if r['id'] != 'base':
                self.assertIn(r['archive'] + '.lha', section, r['id'])
        for heading in ("CLAUDE", "CLAUDE FROM THE AMIGA", "AMIGA-PI"):
            self.assertNotIn("\n" + heading + "\n", readme)
        for kw in ("PYTHON", "NVIM", "REMOTE"):
            self.assertNotIn("[" + kw, readme)

    def test_the_base_version_file_is_installed_by_the_tools_part(self):
        self.assertIn('Copy UP-Term.version UP-Term:UP-Term.version CLONE', dict(dos_parts())['tools'])
        self.assertRegex((ROOT / "dist/parts.txt").read_text(), r"(?m)^tools\s+Files/UP-Term.version$")


class WasabiDaemonOption(unittest.TestCase):
    """wasabid 0.10 is the ONE daemon option of the kit's Install (off by default): it replaces the
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

    def test_the_boot_page_offers_wasabi_0_10b4_off_by_default_and_names_the_key(self):
        a = INSTALLER.index('(set #boot')
        page = INSTALLER[a:INSTALLER.index('(if (in #boot 0)', a)]
        self.assertIn('(default 0)', page)
        self.assertIn('wasabi 0.10', page)
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
        self.assertIn('Search >NIL: C:wasabid "wasabid 0.10"', blk)        # the version marks the old one
        self.assertLess(blk.index('Break $upw C'), blk.index('Copy wasabi/wasabid C:wasabid CLONE'))
        rm = blk[blk.index('If NOT "$optwasabi" EQ "WASABI"'):blk.index('UnSet >NIL: oldwasabid')]
        self.assertIn('If "$oldwasabid" EQ "1"', rm)                       # unticked removes only the old one
        self.assertIn('Delete >NIL: C:wasabid ENVARC:up-term/wasabid QUIET', rm)
        self.assertRegex(blk, r'(?im)^\s*Run >NIL: C:wasabid')            # a ticked install starts the new one

    def test_the_drawer_holds_licence_and_source_note_but_no_binary(self):
        d = ROOT / "dist/wasabi"
        self.assertEqual(sorted(p.name for p in d.iterdir()), ["LICENSE", "SOURCE.txt"])
        self.assertIn("MIT License", (d / "LICENSE").read_text())
        self.assertIn(self.PIN, (d / "SOURCE.txt").read_text())            # the pin of the build is the one named
        self.assertIn("wasabid 0.10", (d / "SOURCE.txt").read_text())
        lic = (ROOT / "dist/LICENSES.txt").read_text(encoding="latin-1")
        self.assertIn("wasabi 0.10", lic)
        self.assertIn("MIT", lic[lic.index("wasabi 0.10"):][:200])
        self.assertNotIn("amiagent", lic)
        self.assertIn("wasabi_mcp.py", (ROOT / "dist/README.txt").read_text(encoding="latin-1"))
        mk = (ROOT / "Makefile").read_text()
        self.assertIn("cp $(BUILD)/wasabi/wasabid $(BUILD)/wasabi/wasabi $(BUILD)/wasabi/wasabi_mcp.py $(BUILD)/wasabi/wasabi_client.py", mk)
        self.assertNotIn("dist/net/amiagent", mk)
        self.assertRegex((ROOT / "dist/parts.txt").read_text(), r"(?m)^boot\s+Files/wasabi$")

    def test_uninstall_stops_and_removes_both_daemons_before_the_drawer_goes(self):
        un = mi.build(ROOT / "dist/Uninstall", "base")
        stop = un.index("If EXISTS ENVARC:up-term/amiagent")
        self.assertIn("Break $upw C", un[stop:stop + 400])
        self.assertLess(stop, un.index('Delete >NIL: "$updest" ALL QUIET'))
        self.assertLess(stop, un.index("Delete >NIL: ENV:up-term ENVARC:up-term ALL QUIET"))
        self.assertIn("Delete >NIL: C:wasabid QUIET", un[un.index("If EXISTS ENVARC:up-term/wasabid"):][:100])


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


class LinkedEnvOn32(unittest.TestCase):
    """UNINSTALL-HANGS-ON-32: AmigaOS 3.2 links RAM:ENV to ENVARC:, so a drawer made in
    ENVARC: shows in ENV: at once; a Copy or MakeDir into that linked drawer leaves one whose
    Delete ALL never returns and stops the RAM disk (Uninstall in the boot of the Install hung,
    2026-10-08). Every ENV: drawer the kit fills is deleted first in the same script, and
    Uninstall deletes the ENV: drawer before the ENVARC: one (whose source is in use)."""

    def test_every_env_drawer_install_fills_is_deleted_first(self):
        lines = [l.strip() for l in DOS.splitlines() if not l.strip().startswith(';')]
        filled = set()
        for i, l in enumerate(lines):
            m = (re.match(r'(?i)copy\s+\S+\s+env:([\w.-]+)\s.*\ball\b', l) or
                 re.match(r'(?i)makedir\s+(?:>nil:\s+)?env:([\w.-]+)\s*$', l))
            # the drawers Uninstall deletes (ENV:Claude is the user's and stays)
            if not m or m.group(1).lower() in filled or m.group(1).lower() not in ('up-term', 'vsh'):
                continue
            name = m.group(1).lower()
            before = [p.lower() for p in lines[:i]]
            self.assertTrue(any(re.match(r'delete\s+(?:>nil:\s+)?env:%s\s+all\b' % re.escape(name), p)
                                for p in before),
                            'install.dos fills ENV:%s without deleting it first: %s' % (name, l))
            filled.add(name)
        self.assertEqual(filled, {'up-term', 'vsh'})

    def test_uninstall_deletes_env_before_envarc(self):
        un = mi.build(ROOT / "dist/Uninstall", "base")
        for name in ('up-term', 'vsh'):
            both = [l.lower() for l in un.splitlines() if l.lower().startswith('delete')
                    and re.search(r'\benv:%s\s' % name, l.lower()) and re.search(r'\benvarc:%s\s' % name, l.lower())]
            self.assertEqual(len(both), 1, name)
            self.assertLess(both[0].index(' env:%s' % name), both[0].index(' envarc:%s' % name), both[0])


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


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
