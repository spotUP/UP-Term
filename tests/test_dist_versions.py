#!/usr/bin/env python3
"""The kit says what it was built from: tools/mkversions.sh writes one line per
consumed part, marks a part with uncommitted changes "+dirty" (warning on
stderr, exit 0), and the built kit archive carries Files/VERSIONS
(plan 2026-10-06-meta-repo, Phase 4)."""
import os, shutil, subprocess, sys, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SCRIPT = os.path.join(ROOT, 'tools', 'mkversions.sh')
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
import pkgs
PARTS = ['vtcon', 'amiga-pi', 'ixemul-vtcon', 'upterm-ports', 'neovim-amiga', 'tmux-amiga',
         'cpython-amiga', 'screen-amiga/src']


def git(d, *a):
    subprocess.run(['git', '-C', d, '-c', 'user.name=t', '-c', 'user.email=t@t'] + list(a),
                   check=True, capture_output=True)


def workspace(tmp):
    for p in PARTS + ['upterm']:
        d = os.path.join(tmp, p)
        os.makedirs(d)
        git(d, 'init', '-q', '-b', 'main')
        open(os.path.join(d, 'f'), 'w').write(p)
        git(d, 'add', 'f')
        git(d, 'commit', '-q', '-m', 'x')


class Versions(unittest.TestCase):
    def run_script(self, root):
        # the script reads vtcon from its own checkout, the parts from root
        return subprocess.run(['sh', SCRIPT, root], capture_output=True, text=True)

    def test_a_line_per_consumed_part_and_a_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            workspace(tmp)
            r = self.run_script(tmp)
            self.assertEqual(r.returncode, 0, r.stderr)
            lines = r.stdout.splitlines()
            self.assertTrue(lines[0].startswith('UP-Term '), lines[0])
            self.assertIn('(upterm ', lines[0])
            self.assertEqual([l.split()[0] for l in lines[1:]], PARTS)
            for l in lines[1:]:
                if not l.startswith('vtcon '):
                    self.assertRegex(l, r'^\S+ main [0-9a-f]{7,}$')

    def test_dirty_part_is_marked_and_warned_not_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            workspace(tmp)
            open(os.path.join(tmp, 'tmux-amiga', 'f'), 'w').write('changed')
            r = self.run_script(tmp)
            self.assertEqual(r.returncode, 0)
            self.assertIn('+dirty', [l for l in r.stdout.splitlines() if l.startswith('tmux-amiga ')][0])
            self.assertIn('tmux-amiga', r.stderr)
            clean = [l for l in r.stdout.splitlines() if l.startswith('neovim-amiga ')][0]
            self.assertNotIn('dirty', clean)

    def test_missing_part_is_named_not_skipped(self):
        with tempfile.TemporaryDirectory() as tmp:
            workspace(tmp)
            shutil.rmtree(os.path.join(tmp, 'cpython-amiga'))
            r = self.run_script(tmp)
            self.assertIn('cpython-amiga none unknown', r.stdout)


    def test_package_header_carries_archive_and_version_and_parts_are_the_ones_given(self):
        """K2-7.4: mkversions.sh --package <archive> <version> [root [part ...]] for an add-on's repositories."""
        with tempfile.TemporaryDirectory() as tmp:
            workspace(tmp)
            r = subprocess.run(['sh', SCRIPT, '--package', 'UP-Term-nvim', '1.2', tmp, 'neovim-amiga', 'ixemul-vtcon'],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            lines = r.stdout.splitlines()
            self.assertRegex(lines[0], r'^UP-Term-nvim 1\.2 \S+ \d{4}-\d\d-\d\d \(upterm [0-9a-f]+\)$')
            self.assertEqual([l.split()[0] for l in lines[1:]], ['neovim-amiga', 'ixemul-vtcon'])

    def test_no_arguments_prints_todays_lines(self):
        """round trip: a call without --package keeps the single kit's header and parts"""
        with tempfile.TemporaryDirectory() as tmp:
            workspace(tmp)
            r = self.run_script(tmp)
            self.assertRegex(r.stdout.splitlines()[0], r'^UP-Term [0-9a-f]+\S* \d{4}-\d\d-\d\d \(upterm ')
            self.assertEqual([l.split()[0] for l in r.stdout.splitlines()[1:]], PARTS)

    def test_every_package_names_the_repositories_it_is_built_from(self):
        self.assertEqual(pkgs.get('base', 'consumes').split(','),
                         ['vtcon', 'ixemul-vtcon', 'upterm-ports', 'tmux-amiga', 'screen-amiga/src'])
        for r in pkgs.rows():
            self.assertTrue(r['consumes'] != '-', r['id'])


# KIT=build/dist/<archive> (make dist-check runs this once for each archive of dist/packages.txt)
KIT = os.environ.get('KIT', os.path.join(ROOT, 'build/dist/UP-Term'))
ARCHIVE = os.path.basename(KIT.rstrip('/'))
LHA = os.path.join(ROOT, 'build', ARCHIVE + '.lha')


@unittest.skipUnless(os.path.exists(LHA) and shutil.which('lha'), 'no built kit (make dist)')
class BuiltKit(unittest.TestCase):
    def test_kit_archive_and_drawer_carry_versions_with_every_part(self):
        out = subprocess.run(['lha', 'l', LHA], capture_output=True, text=True, check=True).stdout
        self.assertIn(ARCHIVE + '/Files/VERSIONS', out)
        row = [r for r in pkgs.rows() if r['archive'] == ARCHIVE][0]
        txt = open(os.path.join(KIT, 'Files/VERSIONS')).read().splitlines()
        self.assertEqual(txt[0].split()[:2], [ARCHIVE, row['version']])
        self.assertEqual([l.split()[0] for l in txt[1:]], row['consumes'].split(','))


if __name__ == '__main__':
    unittest.main()
