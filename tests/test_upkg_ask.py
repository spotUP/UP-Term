#!/usr/bin/env python3
"""dist/upkg-ask (upkg plan P10.6): an add-on's install questions asked through
dialog, through plain prompts, or answered by their defaults with no terminal.
Each case runs under the host's /bin/sh and under build/vsh_host (vsh is /bin/sh
on the Amiga). A fake dialog on the PATH reads "RC VALUE" lines from its stdin,
writes VALUE to stderr and exits RC, and logs its argv. The rig half (real dialog,
keys sent to an UP-Term window) is tools/rig/upkg_ask_rig.py."""
import os, pty, shutil, subprocess, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ASK = os.path.join(ROOT, 'dist', 'upkg-ask')
VSH = os.path.join(ROOT, 'build', 'vsh_host')

FAKE_DIALOG = '''#!/bin/sh
printf '%s\\n' "$*" >> "$DIALOG_LOG"
IFS=' ' read -r rc value
printf '%s' "$value" >&2
exit "$rc"
'''


class Ask(unittest.TestCase):
    shells = [['/bin/sh']] + ([[VSH]] if os.path.exists(VSH) else [])

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='upkgask')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.bin = os.path.join(self.tmp, 'bin')
        os.mkdir(self.bin)
        self.log = os.path.join(self.tmp, 'dialog.log')
        self.remote = os.path.join(self.tmp, 'remote')
        with open(self.remote, 'w') as f:
            f.write('; Claude Code on another computer\n; a comment\n\n10.0.0.5 7777\n')
        self.q = os.path.join(self.tmp, 'questions')
        with open(self.q, 'w') as f:
            f.write('# Claude add-on\n'
                    '\n'
                    'REMOTE|string|Server host and port|<%s|\n'
                    'START|yesno|Start at boot?|no|\n'
                    'MODEL|choice|Model|sonnet|haiku,sonnet,opus\n' % self.remote)
        self.out = os.path.join(self.tmp, 'answers')

    def fake_dialog(self):
        p = os.path.join(self.bin, 'dialog')
        with open(p, 'w') as f:
            f.write(FAKE_DIALOG)
        os.chmod(p, 0o755)

    def run_ask(self, shell, typed=None, plain=False):
        env = {'PATH': self.bin + ':/usr/bin:/bin', 'DIALOG_LOG': self.log, 'HOME': self.tmp}
        if plain:
            env['UPKG_ASK_PLAIN'] = '1'
        if typed is None:
            r = subprocess.run(shell + [ASK, self.q, self.out], stdin=subprocess.DEVNULL,
                               capture_output=True, env=env, timeout=20)
        else:
            master, slave = pty.openpty()
            os.write(master, typed.encode())
            r = subprocess.run(shell + [ASK, self.q, self.out], stdin=slave,
                               capture_output=True, env=env, timeout=20)
            os.close(slave)
            os.close(master)
        self.assertEqual(r.returncode, 0, r.stderr.decode())
        return r

    def answers(self, shell):
        """the answers as postinst sees them: sourced by the same shell"""
        src = '. "%s"; printf "%%s|%%s|%%s" "$REMOTE" "$START" "$MODEL"' % self.out
        return subprocess.run(shell + ['-c', src], capture_output=True,
                              timeout=20).stdout.decode()

    def test_no_terminal_takes_defaults(self):
        self.fake_dialog()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh)
                self.assertEqual(self.answers(sh), '10.0.0.5 7777|no|sonnet')
                self.assertFalse(os.path.exists(self.log), 'dialog ran with no terminal')

    def test_missing_default_file_is_empty(self):
        os.remove(self.remote)
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh)
                self.assertEqual(self.answers(sh), '|no|sonnet')

    def test_dialog_answers(self):
        self.fake_dialog()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                if os.path.exists(self.log):
                    os.remove(self.log)
                self.run_ask(sh, "0 host.lan 2323\n0\n0 opus\n")
                self.assertEqual(self.answers(sh), 'host.lan 2323|yes|opus')
                with open(self.log) as f:
                    calls = f.read().splitlines()
                self.assertEqual(calls[0], '--inputbox Server host and port 0 0 10.0.0.5 7777')
                self.assertEqual(calls[1], '--defaultno --yesno Start at boot? 0 0')
                self.assertEqual(calls[2], '--no-items --default-item sonnet --menu Model 0 0 0 '
                                           'haiku  sonnet  opus ')

    def test_dialog_cancel_and_escape_keep_defaults(self):
        self.fake_dialog()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "1 ignored\n255\n255 haiku\n")
                self.assertEqual(self.answers(sh), '10.0.0.5 7777|no|sonnet')

    def test_dialog_no_is_no(self):
        self.fake_dialog()
        with open(self.q, 'w') as f:
            f.write('START|yesno|Start at boot?|yes|\n')
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "1\n")
                with open(self.out) as f:
                    self.assertEqual(f.read(), "START='no'\n")

    def test_plain_prompts(self):
        """no dialog on the PATH: plain prompts on the terminal"""
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_ask(sh, "host.lan 2323\ny\nopus\n")
                self.assertEqual(self.answers(sh), 'host.lan 2323|yes|opus')
                self.assertIn(b'Server host and port [10.0.0.5 7777]: ', r.stdout)
                self.assertIn(b'Model (haiku,sonnet,opus) [sonnet]: ', r.stdout)

    def test_plain_empty_and_bad_choice_keep_defaults(self):
        self.fake_dialog()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "\nmaybe\ngpt\n", plain=True)
                self.assertEqual(self.answers(sh), '10.0.0.5 7777|no|sonnet')
                self.assertFalse(os.path.exists(self.log), 'UPKG_ASK_PLAIN=1 still ran dialog')

    def test_quotes_survive_sourcing(self):
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "it's a \"host\" $HOME `x`\n\n\n")
                self.assertEqual(self.answers(sh), 'it\'s a "host" $HOME `x`|no|sonnet')

    def test_usage(self):
        r = subprocess.run(['/bin/sh', ASK, self.q], capture_output=True)
        self.assertEqual(r.returncode, 2)


if __name__ == '__main__':
    unittest.main()
