#!/usr/bin/env python3
"""The kit says what it was built from: tools/mkversions.sh writes one line per
consumed part, marks a part with uncommitted changes "+dirty" (warning on
stderr, exit 0), and the built kit archive carries Files/VERSIONS
(plan 2026-10-06-meta-repo, Phase 4)."""
import os, shutil, subprocess, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SCRIPT = os.path.join(ROOT, 'tools', 'mkversions.sh')
PARTS = ['vtcon', 'ixemul-vtcon', 'upterm-ports', 'neovim-amiga', 'tmux-amiga',
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


@unittest.skipUnless(os.path.exists(os.path.join(ROOT, 'build/UP-Term.lha')) and shutil.which('lha'),
                     'no built kit (make dist)')
class BuiltKit(unittest.TestCase):
    def test_kit_archive_and_drawer_carry_versions_with_every_part(self):
        out = subprocess.run(['lha', 'l', os.path.join(ROOT, 'build/UP-Term.lha')],
                             capture_output=True, text=True, check=True).stdout
        self.assertIn('UP-Term/Files/VERSIONS', out)
        txt = open(os.path.join(ROOT, 'build/dist/UP-Term/Files/VERSIONS')).read().splitlines()
        self.assertEqual([l.split()[0] for l in txt[1:]], PARTS)


if __name__ == '__main__':
    unittest.main()
