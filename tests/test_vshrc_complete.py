#!/usr/bin/env python3
"""Archive completion rules of dist/vshrc. vsh's complete has no -X, so one -F
function (_archive_complete) keeps only the archives the command reads, and
the drawers are offered only when no archive matches: "lha x H<Tab>" in a drawer holding HiP/ and
HiP.lha offers only HiP.lha. The real dist/vshrc is sourced by the host vsh
(build/vsh_host). The rig half: lha x H + Tab on rig 8 (2026-10-26)."""
import os, subprocess, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
VSH = os.path.join(ROOT, 'build', 'vsh_host')
VSHRC = os.path.join(ROOT, 'dist', 'vshrc')
CMDS = ('lha', 'lzx', 'unzip', 'tar', 'gunzip', 'unxz')


@unittest.skipUnless(os.path.exists(VSH), 'build/vsh_host missing (make build/vsh_host)')
class ArchiveComplete(unittest.TestCase):
    def run_vsh(self, script, files=()):
        with tempfile.TemporaryDirectory() as d:
            os.mkdir(os.path.join(d, 'HiP-spot-cfec68a'))
            for f in files:
                open(os.path.join(d, f), 'w').close()
            p = subprocess.run([VSH, '-c', '. %s >/dev/null 2>&1; %s' % (VSHRC, script)], cwd=d,
                               stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=30)
        return p.stdout

    def test_every_archiver_has_the_rule(self):
        out = self.run_vsh('complete -p')
        for c in CMDS:
            self.assertIn('complete -F _archive_complete %s\n' % c, out)

    def test_only_archives_of_the_command_are_offered_in_any_case(self):
        files = ('HiP-spot-cfec68a.lha', 'Hx.LHA', 'Hz.txt', 'Ht.tar.gz')
        reply = lambda c: self.run_vsh('_archive_complete %s H x; echo "${COMPREPLY[@]}"' % c, files).split()
        self.assertEqual(reply('lha'), ['HiP-spot-cfec68a.lha', 'Hx.LHA'])
        self.assertEqual(reply('tar'), ['Ht.tar.gz'])
        self.assertEqual(reply('unzip'), ['HiP-spot-cfec68a'])  # no archive: the drawer, to walk into

    def test_a_command_without_a_rule_keeps_the_default(self):
        self.assertNotIn('echo', self.run_vsh('complete -p'))


if __name__ == '__main__':
    unittest.main()
