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
INSTALLER = (ROOT / "dist/Install.installer").read_text(encoding="latin-1")
DOS = (ROOT / "dist/install.dos").read_text(encoding="latin-1")

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
        missing = [n for n in names if not re.search(r'"%s[="]' % n, INSTALLER)]
        self.assertEqual(missing, [], 'install.dos keywords the Installer never passes')


class ClaudeRemoteFile(unittest.TestCase):
    """CLAUDE-REMOTE-KEPT: the Installer's server page starts with the host and
    port of an existing ENVARC:Claude/remote, and install.dos writes the chosen
    value (a kept file ignored the owner's answer)."""
    FILE = ("; Claude Code on another computer: host and port. With no API key,\n"
            "; plain Claude connects there (uptelnet). Delete this file to stop.\n"
            "192.168.0.198 2323\n")

    def test_page_default_is_the_existing_files_host_line(self):
        self.assertIn('(default #oldremote)', INSTALLER)
        self.assertNotIn('(default "192.168.0.198 2323")', INSTALLER)
        self.assertIn('(set #oldremote "")', INSTALLER)
        self.assertNotIn('192.168.', INSTALLER, 'no private LAN address as a default')
        self.assertIn('(set #remotetext #remote)', INSTALLER)
        m = re.search(r'\(run "Search ENVARC:Claude/remote \\"([^"]*?)\\" PATTERN NONUM >ENV:UPTermRemote"', INSTALLER)
        self.assertTrue(m, 'the read of the existing file')
        # the Search pattern, in Python terms: not a comment line, a space, a port
        self.assertEqual(m.group(1), '[~;]#? #[0-9]#?')
        host = [l for l in self.FILE.splitlines() if re.match(r'[^;].* [0-9]', l)]
        self.assertEqual(host, ['192.168.0.198 2323'], 'comment lines are skipped')
        # the read only happens when the file exists, before the page asks
        self.assertLess(INSTALLER.index('(exists "ENVARC:Claude/remote")'), INSTALLER.index('(set #remote\n'))

    def test_install_writes_the_chosen_remote_even_when_the_file_exists(self):
        a = DOS.index('If NOT "{REMOTE}" EQ ""')
        blk = DOS[a:DOS.index('If NOT "{STEP}" EQ ""', a)]
        self.assertNotIn('If NOT EXISTS ENVARC:Claude/remote', blk, 'an existing file must not block the write')
        want = ['Echo >ENVARC:Claude/remote "; Claude Code on another computer: host and port. With no API key,"',
                'Echo >>ENVARC:Claude/remote "; plain Claude connects there (uptelnet). Delete this file to stop."',
                'Echo >>ENVARC:Claude/remote "{REMOTE}"']
        lines = [l.strip() for l in blk.splitlines()]
        for w in want:
            self.assertIn(w, lines)
        self.assertEqual([lines.index(w) for w in want], sorted(lines.index(w) for w in want))
        self.assertIn('Copy ENVARC:Claude/remote ENV:Claude/remote CLONE QUIET', lines)


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
