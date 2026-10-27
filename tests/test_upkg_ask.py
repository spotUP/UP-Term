#!/usr/bin/env python3
"""dist/upkg-ask (upkg plan P10.6): an add-on's install questions asked through
upkg-ui, through plain prompts, or answered by their defaults with no terminal.
Each case runs under the host's /bin/sh and under build/vsh_host (vsh is /bin/sh
on the Amiga). A fake upkg-ui on the PATH reads "RC VALUE" lines from its stdin,
writes VALUE to stderr and exits RC, and logs its argv. One case runs the REAL
host-built upkg-ui (upterm-ports build/host/upkg-ui) on a pty with keys sent.
The rig half (keys sent to an UP-Term window) is tools/rig/upkg_ask_rig.py."""
import os, pty, select, shutil, struct, fcntl, termios, subprocess, sys, tempfile, time, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ASK = os.path.join(ROOT, 'dist', 'upkg-ask')
sys.path.insert(0, os.path.join(ROOT, 'tools', 'rig'))
import paths
REAL_UI = str(paths.repo('upterm-ports') / 'build/host/upkg-ui/upkg-ui')
TITLE = '--title UP-TERM ADD-ON --backtitle '  # upkg-ask puts these before every call
VSH = os.path.join(ROOT, 'build', 'vsh_host')

FAKE_UI = '''#!/bin/sh
printf '%s\\n' "$*" >> "$UI_LOG"
IFS=' ' read -r rc value
printf '%s' "$value" >&2
exit "$rc"
'''

# ncurses clear: logged, never drawn
FAKE_CLEAR = '''#!/bin/sh
echo clear >> "${CLEAR_LOG:-/dev/null}"
'''


class Ask(unittest.TestCase):
    shells = [['/bin/sh']] + ([[VSH]] if os.path.exists(VSH) else [])

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='upkgask')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.bin = os.path.join(self.tmp, 'bin')
        os.mkdir(self.bin)
        self.log = os.path.join(self.tmp, 'ui.log')
        self.clear_log = os.path.join(self.tmp, 'clear.log')
        with open(os.path.join(self.bin, 'clear'), 'w') as f:
            f.write(FAKE_CLEAR)
        os.chmod(os.path.join(self.bin, 'clear'), 0o755)
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

    def fake_ui(self):
        p = os.path.join(self.bin, 'upkg-ui')
        with open(p, 'w') as f:
            f.write(FAKE_UI)
        os.chmod(p, 0o755)

    def run_ask(self, shell, typed=None, plain=False, state=None):
        env = {'PATH': self.bin + ':/usr/bin:/bin', 'UI_LOG': self.log, 'CLEAR_LOG': self.clear_log, 'HOME': self.tmp}
        if plain:
            env['UPKG_ASK_PLAIN'] = '1'
        if state:
            env['UPKG_ASK_STATE'] = state
        if typed is None:
            r = subprocess.run(shell + [ASK, self.q, self.out], stdin=subprocess.DEVNULL,
                               capture_output=True, env=env, timeout=20)
        else:
            # stdin and stdout on the pty, as in a Shell window (stdout into a pipe takes defaults)
            master, slave = pty.openpty()
            os.write(master, typed.encode())
            r = subprocess.run(shell + [ASK, self.q, self.out], stdin=slave, stdout=slave,
                               stderr=subprocess.PIPE, env=env, timeout=20)
            # read before the slave closes: macOS drops what the master has not read by then
            r.stdout = b''
            while select.select([master], [], [], 0.1)[0]:
                try:
                    data = os.read(master, 65536)
                except OSError:
                    break
                if not data:
                    break
                r.stdout += data
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
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh)
                self.assertEqual(self.answers(sh), '10.0.0.5 7777|no|sonnet')
                self.assertFalse(os.path.exists(self.log), 'upkg-ui ran with no terminal')

    def test_output_into_a_pipe_takes_defaults(self):
        """regression (owner, 2026-10-22): upkg browse ran "upkg install up-claude" into a programbox
        pipe; upkg-ask saw a terminal on stdin and drew upkg-ui into the pipe, so the questions showed
        as escape-code text and the install waited for keys to a question nobody could see"""
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                if os.path.exists(self.log):
                    os.remove(self.log)
                master, slave = pty.openpty()
                os.write(master, b"0 host.lan 2323\n0\n0 opus\n")
                env = {'PATH': self.bin + ':/usr/bin:/bin', 'UI_LOG': self.log, 'HOME': self.tmp}
                r = subprocess.run(sh + [ASK, self.q, self.out], stdin=slave, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, env=env, timeout=20)
                os.close(slave)
                os.close(master)
                self.assertEqual(r.returncode, 0, r.stderr.decode())
                self.assertEqual(self.answers(sh), '10.0.0.5 7777|no|sonnet')
                self.assertFalse(os.path.exists(self.log), 'upkg-ui ran with its output in a pipe')

    def test_missing_default_file_is_empty(self):
        os.remove(self.remote)
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh)
                self.assertEqual(self.answers(sh), '|no|sonnet')

    def test_ui_answers(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                if os.path.exists(self.log):
                    os.remove(self.log)
                self.run_ask(sh, "0 host.lan 2323\n0\n0 opus\n")
                self.assertEqual(self.answers(sh), 'host.lan 2323|yes|opus')
                with open(self.log) as f:
                    calls = f.read().splitlines()
                self.assertEqual(calls[0], TITLE + '1/3 --inputbox Server host and port 0 0 10.0.0.5 7777')
                self.assertEqual(calls[1], TITLE + '2/3 --defaultno --yesno Start at boot? 0 0')
                self.assertEqual(calls[2], TITLE + '3/3 --no-items --default-item sonnet --menu Model 0 0 0 '
                                           'haiku  sonnet  opus ')

    def test_an_upgrade_asks_only_new_questions(self):
        """upterm-ports PR 14: upkg exports UPKG_ASK_STATE (info_dir/NAME.answered). Questions an earlier
        install answered keep that answer without asking; only the new one is asked, and is recorded"""
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                if os.path.exists(self.log):
                    os.remove(self.log)
                state = os.path.join(self.tmp, 'up-claude.answered')
                with open(state, 'w') as f:
                    f.write("REMOTE='old.lan 2323'\nSTART='yes'\n")
                self.run_ask(sh, "0 opus\n", state=state)
                self.assertEqual(self.answers(sh), 'old.lan 2323|yes|opus')
                with open(self.log) as f:
                    calls = f.read().splitlines()
                self.assertEqual(calls, [TITLE + '1/1 --no-items --default-item sonnet --menu Model 0 0 0 '
                                                 'haiku  sonnet  opus '])
                with open(state) as f:
                    self.assertIn("MODEL='opus'", f.read().splitlines())

    def test_the_last_question_is_cleared_after_upkg_ui(self):
        """regression (owner, 2026-10-21): upkg-ui stays on the normal screen (no Shell flash between
        questions), so the last question is cleared before the install goes on; plain prompts are not"""
        self.fake_ui()
        for sh in self.shells:
            for plain, typed, n in ((False, "0 host.lan 2323\n0\n0 opus\n", 1), (True, "\n\n\n", 0)):
                with self.subTest(sh=sh[0], plain=plain):
                    if os.path.exists(self.clear_log):
                        os.remove(self.clear_log)
                    self.run_ask(sh, typed, plain=plain)
                    n_got = len(open(self.clear_log).read().splitlines()) if os.path.exists(self.clear_log) else 0
                    self.assertEqual(n_got, n)

    def test_ui_cancel_and_escape_keep_defaults(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "1 ignored\n255\n255 haiku\n")
                self.assertEqual(self.answers(sh), '10.0.0.5 7777|no|sonnet')

    def test_ui_no_is_no(self):
        self.fake_ui()
        with open(self.q, 'w') as f:
            f.write('START|yesno|Start at boot?|yes|\n')
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "1\n")
                with open(self.out) as f:
                    self.assertEqual(f.read(), "START='no'\n")

    def test_plain_prompts(self):
        """no upkg-ui on the PATH: plain prompts on the terminal"""
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_ask(sh, "host.lan 2323\ny\nopus\n")
                self.assertEqual(self.answers(sh), 'host.lan 2323|yes|opus')
                self.assertIn(b'Server host and port [10.0.0.5 7777]: ', r.stdout)
                self.assertIn(b'Model (haiku,sonnet,opus) [sonnet]: ', r.stdout)

    def test_plain_empty_and_bad_choice_keep_defaults(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "\nmaybe\ngpt\n", plain=True)
                self.assertEqual(self.answers(sh), '10.0.0.5 7777|no|sonnet')
                self.assertFalse(os.path.exists(self.log), 'UPKG_ASK_PLAIN=1 still ran upkg-ui')

    def test_quotes_survive_sourcing(self):
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_ask(sh, "it's a \"host\" $HOME `x`\n\n\n")
                self.assertEqual(self.answers(sh), 'it\'s a "host" $HOME `x`|no|sonnet')

    def test_name_is_in_the_backtitle(self):
        self.fake_ui()
        env = {'PATH': self.bin + ':/usr/bin:/bin', 'UI_LOG': self.log, 'HOME': self.tmp}
        master, slave = pty.openpty()
        os.write(master, b"0 a\n0\n0 opus\n")
        r = subprocess.run(['/bin/sh', ASK, self.q, self.out, 'Claude'], stdin=slave, stdout=slave,
                           stderr=subprocess.PIPE, env=env, timeout=20)
        os.close(slave)
        os.close(master)
        self.assertEqual(r.returncode, 0, r.stderr.decode())
        with open(self.log) as f:
            self.assertTrue(f.readline().startswith(TITLE + 'Claude 1/3 --inputbox'))

    @unittest.skipUnless(os.path.exists(REAL_UI), 'host upkg-ui missing: run "make host-upkg-ui" in upterm-ports')
    def test_real_upkg_ui_on_a_pty(self):
        """reachability: upkg-ask drives the real ncurses upkg-ui, keys in, answers file out"""
        os.symlink(os.path.abspath(REAL_UI), os.path.join(self.bin, 'upkg-ui'))
        env = {'PATH': self.bin + ':/usr/bin:/bin', 'HOME': self.tmp, 'TERM': 'xterm-256color',
               'LANG': 'en_US.UTF-8'}
        keys = [(b'9', 0.8), (b'\r', 0.3),          # input: append 9 to the default, accept
                (b'\x1bOD', 0.8), (b'\r', 0.3),    # yesno (default no): Left = Yes, accept
                (b'\x1bOB', 0.8), (b'\r', 0.3)]    # menu (default sonnet): Down = opus, accept
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                master, slave = pty.openpty()
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 25, 80, 0, 0))
                p = subprocess.Popen(sh + [ASK, self.q, self.out, 'Claude'], stdin=slave, stdout=slave,
                                     stderr=subprocess.PIPE, env=env, start_new_session=True)
                os.close(slave)
                screen = b''
                for key, wait in keys:
                    end = time.time() + wait
                    while time.time() < end:
                        if select.select([master], [], [], 0.05)[0]:
                            try:
                                screen += os.read(master, 65536)
                            except OSError:
                                break
                    os.write(master, key)
                end = time.time() + 10
                while p.poll() is None and time.time() < end:
                    if select.select([master], [], [], 0.05)[0]:
                        try:
                            screen += os.read(master, 65536)
                        except OSError:
                            break
                err = p.stderr.read().decode()
                p.stderr.close()
                os.close(master)
                self.assertEqual(p.returncode, 0, err)
                self.assertEqual(self.answers(sh), '10.0.0.5 77779|yes|opus')
                # the screen really was upkg-ui's: the masthead carries the title and "NAME i/n"
                self.assertIn(b'UP-TERM ADD-ON', screen)
                self.assertIn(b'Claude 3/3', screen)

    def test_usage(self):
        r = subprocess.run(['/bin/sh', ASK, self.q], capture_output=True)
        self.assertEqual(r.returncode, 2)


if __name__ == '__main__':
    unittest.main()
