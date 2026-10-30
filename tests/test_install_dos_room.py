#!/usr/bin/env python3
"""SHELL-INSTALL-DISK-FULL: the Shell install (dist/install.dos, no STEP) checks room before it copies anything.

UPBigVol ROOM gets the new drawer and SYS: (and the AmiSSL disk when AmiSSL will be installed), each followed by the
KB of every part that writes there, an option's KB only when the option is on. WARN quits before the first Copy
and leaves no UP-Term: assign. install/bigvol.h (tests/test_bigvol.c) groups and merges the words."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import kitsize
import mkinstaller as mi

TEMPLATE = (ROOT / 'dist/install.dos').read_text(encoding='latin-1')
DOS = mi.build(ROOT / 'dist/install.dos', 'base')
# the parts of the drawer's disk and of SYS:, as install.dos and Install.installer's room check split them
DEST = ['drawer', 'copy-coreutils', 'copy-userland', 'userland', 'copy-unifont', 'copy-emoji', 'tools', 'unix',
        'record']
SYS = ['system', 'fonts', 'desktop', 'boot', 'libs', 'mount', 'console']
# option: (its install.dos variable and value, the local variable that holds its KB)
OPTION = {'KBCOPYUNIFONT': ('optunifont', 'UNIFONT', 'upkbuni'), 'KBCOPYEMOJI': ('optemoji', 'EMOJI', 'upkbemo'),
          'KBSSH': ('optssh', 'SSH', 'upkbssh'), 'KBBEBBOGET': ('optbebboget', 'BEBBOGET', 'upkbbg'),
          'KBWASABI': ('optwasabi', 'WASABI', 'upkbwas')}


def kb(part):
    return 'KB' + part.upper().replace('-', '')


def drawer_part():
    return TEMPLATE[TEMPLATE.index('Lab drawer'):TEMPLATE.index('Lab system')]


def room_line():
    return next(l.strip() for l in drawer_part().splitlines() if 'UPBigVol" ROOM' in l)


class InstallDosRoom(unittest.TestCase):
    def test_every_part_is_counted_once_on_its_disk(self):
        line = room_line()
        words = line.split()
        self.assertEqual(words[:3], ['"{DIR}/UPBigVol"', 'ROOM', '"$updest"'])
        sys_at = words.index('SYS:')
        want = {'DEST': [kb(p) for p in DEST] + ['KBSSH', 'KBBEBBOGET'], 'SYS': [kb(p) for p in SYS] + ['KBWASABI']}
        got = {'DEST': words[3:sys_at], 'SYS': words[sys_at + 1:]}
        for disk, names in want.items():
            for name in names:
                token = '$' + OPTION[name][2] if name in OPTION else '@%s@' % name
                self.assertEqual(got[disk].count(token), 1, '%s not counted once on %s' % (name, disk))
        # every Lab part of install.dos is in one of the two lists (record has no Lab: it writes the record at the
        # end), so a new part cannot slip past the check
        labs = re.findall(r'^Lab (\S+)', TEMPLATE, re.M)
        self.assertEqual(sorted(labs), sorted(set(DEST + SYS) - {'record'}))
        self.assertEqual(got['SYS'][-1], '$upssl')

    def test_every_value_it_names_is_a_kitsize_value(self):
        names = set(re.findall(r'@(KB[A-Z0-9]+)@', drawer_part()))
        values = kitsize.sizes(ROOT / 'build/dist/no-such-kit', 'base')
        self.assertEqual(sorted(names - set(values)), [])
        self.assertNotIn('@KB', DOS[DOS.index('Lab drawer'):DOS.index('Lab system')])

    def test_an_option_counts_only_when_it_is_on(self):
        part = drawer_part()
        for name, (var, value, local) in OPTION.items():
            block = 'Set %s 0\n    If "$%s" EQ "%s"\n      Set %s @%s@\n    EndIf' % (local, var, value, local, name)
            self.assertIn(block, part)

    def test_amissl_counts_on_the_disk_install_amissl_picks_only_when_it_will_run(self):
        part = drawer_part()
        ssl = part[part.index('Set upssl ""'):part.index('UPBigVol" ROOM')]
        self.assertIn('Version >NIL: LIBS:amisslmaster.library 5\n    If WARN', ssl)
        order = [ssl.index('Set upssl "%s @KBAMISSL@"' % d) for d in ('SYS:', 'Work:', 'AmiSSL:')]
        self.assertEqual(order, sorted(order), 'the later Set wins: SYS:, then Work:, then the AmiSSL: assign')
        self.assertIn('Assign >NIL: Work: EXISTS', ssl)
        self.assertIn('Assign >NIL: AmiSSL: EXISTS', ssl)

    def test_only_a_shell_install_of_a_new_drawer_and_a_refusal_copies_nothing(self):
        part = drawer_part()
        guard = 'If "{STEP}" EQ ""\n  If NOT EXISTS "$updest/VERSIONS"\n'
        at = part.index(guard)
        self.assertLess(part.index('Set updest SYS:UP-Term'), at)
        self.assertLess(at, part.index('UPBigVol" ROOM'))
        refuse = part[part.index('UPBigVol" ROOM'):part.index('MakeDir')]
        self.assertIn('If WARN\n      Echo "UP-Term: nothing has been copied.', refuse)
        self.assertIn('UnSet updest\n      Quit 20\n', refuse)
        # nothing before the check copies, makes a drawer or assigns UP-Term:
        before = TEMPLATE[:TEMPLATE.index('UPBigVol" ROOM')]
        for word in ('Copy ', 'MakeDir ', 'Assign UP-Term:'):
            self.assertNotIn('\n' + word, before.replace('\n  ', '\n').replace('\n    ', '\n'))


if __name__ == '__main__':
    unittest.main()
