#!/usr/bin/env python3
"""TMUX-PANE-PATH (2026-11-03): in a tmux pane vsh said "vsh: sleep: not found". tmux (tmux/spawn.c) gives
every pane PATH=/usr/bin:/bin when the server has none, and dist/vshrc only set the kit PATH when PATH was
empty, so the pane kept a PATH without /UP-Term/bin. A PATH that is just the tmux/ixemul default counts as
unset; any other PATH the user set is left alone. The real dist/vshrc is sourced by the host vsh."""
import os, subprocess, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
VSH = os.path.join(ROOT, 'build', 'vsh_host')
VSHRC = os.path.join(ROOT, 'dist', 'vshrc')


@unittest.skipUnless(os.path.exists(VSH), 'build/vsh_host missing (make build/vsh_host)')
class InheritedPath(unittest.TestCase):
    def path(self, value):
        e = {k: v for k, v in os.environ.items() if k != 'PATH'}
        if value is not None:
            e['PATH'] = value
        src = '. %s >/dev/null 2>&1; echo "P=$PATH"' % VSHRC
        p = subprocess.run([VSH, '-c', src], env=e, stdin=subprocess.DEVNULL,
                           capture_output=True, text=True, timeout=30)
        return p.stdout.strip().split('P=', 1)[-1]

    def test_tmux_pane_default_gets_the_kit_drawers(self):
        for d in ('/usr/bin:/bin', '/usr/bin:/bin:/c'):
            self.assertTrue(self.path(d).startswith('/UP-Term/bin:'), d)

    def test_empty_path_gets_the_kit_drawers(self):
        self.assertTrue(self.path(None).startswith('/UP-Term/bin:'))

    def test_a_path_the_user_set_is_left_alone(self):
        self.assertEqual(self.path('/mine/bin:/c'), '/mine/bin:/c')


if __name__ == '__main__':
    unittest.main()
