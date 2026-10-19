#!/usr/bin/env python3
"""EDITOR default of dist/vshrc (upkg P9-9). vim left the base archive for
the upkg feed and the base has no vi, so fc and git commit, which fall back
to vi, found no editor. dist/vshrc names joe, which the base ships, when
EDITOR is unset, and leaves an EDITOR the user set alone. The real
dist/vshrc is sourced by the host vsh (build/vsh_host)."""
import os, subprocess, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
VSH = os.path.join(ROOT, 'build', 'vsh_host')
VSHRC = os.path.join(ROOT, 'dist', 'vshrc')


@unittest.skipUnless(os.path.exists(VSH), 'build/vsh_host missing (make build/vsh_host)')
class Editor(unittest.TestCase):
    def editor(self, **env):
        e = {k: v for k, v in os.environ.items() if k not in ('EDITOR', 'FCEDIT')}
        e.update(env)
        src = '. %s >/dev/null 2>&1; echo "E=$EDITOR"; env | grep "^EDITOR="' % VSHRC
        p = subprocess.run([VSH, '-c', src], env=e, stdin=subprocess.DEVNULL,
                           capture_output=True, text=True, timeout=30)
        return p.stdout.split()

    def test_unset_editor_becomes_joe_and_is_exported(self):
        self.assertEqual(self.editor(), ['E=joe', 'EDITOR=joe'])

    def test_editor_the_user_set_is_kept(self):
        self.assertEqual(self.editor(EDITOR='nano'), ['E=nano', 'EDITOR=nano'])


if __name__ == '__main__':
    unittest.main()
