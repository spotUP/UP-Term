#!/usr/bin/env python3
"""dist/upkg-browse (plan 2026-10-21-upkg-browse, Phase B): the "upkg browse" list loop.
A fake upkg (list, list-installed, list-upgradable, install, remove, upgrade, info over state files)
and a fake upkg-ui sit on the PATH. The fake upkg-ui reads "RC VALUE" lines from its stdin, writes
VALUE to stderr, exits RC and logs its argv; the keyboard is a pty with those lines (and the Return
of each pause) typed in. Each case runs under /bin/sh and under build/vsh_host when it is built.
One case runs the REAL host upkg-ui (upterm-ports build/host/upkg-ui) on a pty with keys sent; it
is skipped when that binary is missing or has no --hotkeys."""
import os, pty, select, shutil, struct, fcntl, termios, subprocess, tempfile, time, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BROWSE = os.path.join(ROOT, 'dist', 'upkg-browse')
REAL_UI = os.path.join(ROOT, '..', 'upterm-ports', 'build', 'host', 'upkg-ui', 'upkg-ui')
VSH = os.path.join(ROOT, 'build', 'vsh_host')
HEAD = '--title UP-TERM ADD-ONS --backtitle 3 add-ons --hotkeys iruq '
PROMPT = 'Enter info   i install   r remove   u upgrade   q quit'
ROWS = ('up-vim installed 1.3   Vim, the text editor '
        'up-git available 2.0   Git - the stupid content tracker '
        'up-perl upgrade 5.38 to 5.40   Perl language')

FAKE_UI = '''#!/bin/sh
printf '%s\\n' "$*" >> "$UI_LOG"
IFS=' ' read -r rc value
printf '%s' "$value" >&2
exit "$rc"
'''

FAKE_UPKG = '''#!/bin/sh
case $1 in
list) cat "$STATE/feed"; exit 0 ;;
list-installed) cat "$STATE/installed"; exit 0 ;;
list-upgradable) cat "$STATE/upgradable"; exit 0 ;;
esac
echo "$*" >> "$CMD_LOG"
case $1 in
install) [ -n "$INSTALL_RC" ] && exit "$INSTALL_RC"
    grep "^$2 - " "$STATE/feed" | sed 's/ - [^-]*$//' >> "$STATE/installed" ;;
remove) grep -v "^$2 - " "$STATE/installed" > "$STATE/x"; mv "$STATE/x" "$STATE/installed" ;;
upgrade) grep -v "^$2 - " "$STATE/installed" > "$STATE/x"; mv "$STATE/x" "$STATE/installed"
    echo "$2 - 5.40" >> "$STATE/installed"; : > "$STATE/upgradable" ;;
info) echo "info of $2" ;;
esac
exit 0
'''


def pump(master, seconds, screen=b''):
    end = time.time() + seconds
    while time.time() < end:
        if select.select([master], [], [], 0.05)[0]:
            try:
                screen += os.read(master, 65536)
            except OSError:
                break
    return screen


class Browse(unittest.TestCase):
    shells = [['/bin/sh']] + ([[VSH]] if os.path.exists(VSH) else [])

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='upkgbrowse')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.bin = os.path.join(self.tmp, 'bin')
        os.mkdir(self.bin)
        self.state = os.path.join(self.tmp, 'state')
        os.mkdir(self.state)
        self.ui_log = os.path.join(self.tmp, 'ui.log')
        self.cmd_log = os.path.join(self.tmp, 'cmd.log')
        self.upkg = os.path.join(self.bin, 'upkg')
        self.fill('feed', 'up-vim - 1.3 - Vim, the text editor\n'
                          'up-git - 2.0 - Git - the stupid content tracker\n'
                          'up-perl - 5.40 - Perl language\n')
        self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
        self.fill('upgradable', 'up-perl - 5.38 - 5.40\n')
        with open(self.upkg, 'w') as f:
            f.write(FAKE_UPKG)
        os.chmod(self.upkg, 0o755)

    def fill(self, name, text):
        with open(os.path.join(self.state, name), 'w') as f:
            f.write(text)

    def fake_ui(self):
        p = os.path.join(self.bin, 'upkg-ui')
        with open(p, 'w') as f:
            f.write(FAKE_UI)
        os.chmod(p, 0o755)

    def env(self, **extra):
        env = {'PATH': self.bin + ':/usr/bin:/bin', 'UPKG': self.upkg, 'STATE': self.state,
               'UI_LOG': self.ui_log, 'CMD_LOG': self.cmd_log, 'HOME': self.tmp, 'TMPDIR': self.tmp}
        env.update(extra)
        return env

    def run_browse(self, shell, typed=None, args=(), rc=0, **extra):
        """run on a pty with TYPED waiting on the keyboard (None: stdin is /dev/null)"""
        for f in (self.ui_log, self.cmd_log):
            if os.path.exists(f):
                os.remove(f)
        if typed is None:
            r = subprocess.run(shell + [BROWSE, *args], stdin=subprocess.DEVNULL,
                               capture_output=True, env=self.env(**extra), timeout=20)
        else:
            master, slave = pty.openpty()
            os.write(master, typed.encode())
            r = subprocess.run(shell + [BROWSE, *args], stdin=slave,
                               capture_output=True, env=self.env(**extra), timeout=20)
            os.close(slave)
            os.close(master)
        self.assertEqual(r.returncode, rc, r.stderr.decode())
        return r

    def read(self, path):
        if not os.path.exists(path):
            return []
        with open(path) as f:
            return f.read().splitlines()

    def calls(self):
        return self.read(self.ui_log)

    def commands(self):
        return self.read(self.cmd_log)

    def test_rows_and_markers_split_at_the_first_and_second_separator(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_browse(sh, "0 q up-vim\n")
                self.assertEqual(self.calls(), [HEAD + '--menu ' + PROMPT + ' 0 0 0 ' + ROWS])
                self.assertEqual(self.commands(), [])

    def test_install_runs_the_command_keeps_the_cursor_and_refreshes_the_marker(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                r = self.run_browse(sh, "0 i up-git\n\n0 q up-git\n")
                self.assertEqual(self.commands(), ['install up-git'])
                calls = self.calls()
                self.assertEqual(len(calls), 2)
                self.assertEqual(calls[1], HEAD + '--default-item up-git --menu ' + PROMPT + ' 0 0 0 '
                                 + ROWS.replace('up-git available 2.0', 'up-git installed 2.0'))
                self.assertIn(b'[OK] upkg install up-git', r.stdout)
                self.assertIn(b'Press Return to go back to the list.', r.stdout)

    def test_a_failed_command_says_error_with_its_code(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_browse(sh, "0 i up-git\n\n0 q up-git\n", INSTALL_RC='3')
                self.assertIn(b'[ERROR] upkg install up-git returned 3', r.stdout)
                self.assertIn('up-git available 2.0', self.calls()[1])

    def test_enter_shows_info(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_browse(sh, "0 enter up-vim\n\n0 q up-vim\n")
                self.assertEqual(self.commands(), ['info up-vim'])
                self.assertIn(b'info of up-vim', r.stdout)
                self.assertIn('--default-item up-vim', self.calls()[1])

    def test_remove_asks_first_and_yes_removes(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                r = self.run_browse(sh, "0 r up-vim\n0\n\n0 q up-vim\n")
                calls = self.calls()
                self.assertEqual(calls[1], '--title UP-TERM ADD-ONS --defaultno --yesno Remove up-vim? 0 0')
                self.assertEqual(self.commands(), ['remove up-vim'])
                self.assertIn(b'[OK] upkg remove up-vim', r.stdout)
                self.assertIn('up-vim available 1.3', calls[2])

    def test_remove_answered_no_runs_nothing(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_browse(sh, "0 r up-vim\n1\n0 q up-vim\n")
                self.assertEqual(self.commands(), [])
                self.assertEqual(len(self.calls()), 3)
                self.assertIn('--default-item up-vim', self.calls()[2])

    def test_upgrade_runs_the_command_and_the_marker_becomes_installed(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                self.fill('upgradable', 'up-perl - 5.38 - 5.40\n')
                self.run_browse(sh, "0 u up-perl\n\n0 q up-perl\n")
                self.assertEqual(self.commands(), ['upgrade up-perl'])
                self.assertIn('up-perl installed 5.40   Perl language', self.calls()[1])

    def test_a_key_that_does_not_fit_the_row_prints_one_line_and_runs_nothing(self):
        self.fake_ui()
        cases = [("0 i up-vim\n\n0 q up-vim\n", b'up-vim is already installed.'),
                 ("0 r up-git\n\n0 q up-git\n", b'up-git is not installed.'),
                 ("0 u up-vim\n\n0 q up-vim\n", b'up-vim has no newer version.')]
        for sh in self.shells:
            for typed, line in cases:
                with self.subTest(sh=sh[0], line=line):
                    r = self.run_browse(sh, typed)
                    self.assertIn(line, r.stdout)
                    self.assertEqual(self.commands(), [])
                    self.assertEqual(len(self.calls()), 2)  # no yes/no question either

    def test_quit_and_escape_leave_with_zero(self):
        self.fake_ui()
        for sh in self.shells:
            for typed in ("0 q up-vim\n", "255\n", "1\n"):
                with self.subTest(sh=sh[0], typed=typed):
                    self.run_browse(sh, typed)
                    self.assertEqual(len(self.calls()), 1)
                    self.assertEqual(self.commands(), [])

    def test_no_terminal_is_refused(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_browse(sh, None, rc=2)
                self.assertEqual(r.stderr, b'upkg browse needs a terminal. Use upkg list.\n')
                self.assertEqual(self.calls(), [])

    def test_no_upkg_ui_is_refused(self):
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_browse(sh, "0 q x\n", rc=2)
                self.assertEqual(r.stderr, b'upkg browse needs UP-Term:bin/upkg-ui.\n')

    def test_an_empty_list_says_to_update_and_exits_one(self):
        self.fake_ui()
        self.fill('feed', '')
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_browse(sh, "0 q x\n", rc=1)
                self.assertEqual(r.stderr, b'No package lists. Run upkg update first.\n')
                self.assertEqual(self.calls(), [])

    def test_help_forms_print_usage_and_exit_zero(self):
        for sh in self.shells:
            for arg in ('--help', '-h', 'help', '?'):
                with self.subTest(sh=sh[0], arg=arg):
                    r = self.run_browse(sh, None, args=[arg])
                    self.assertTrue(r.stdout.startswith(b'usage: upkg browse\n'), r.stdout)
                    self.assertIn(b'q quit', r.stdout)

    def test_any_other_argument_is_a_usage_error(self):
        for sh in self.shells:
            for args in (['--bogus'], ['a', 'b']):
                with self.subTest(sh=sh[0], args=args):
                    r = self.run_browse(sh, None, args=args, rc=2)
                    self.assertTrue(r.stderr.startswith(b'usage: upkg browse\n'))
                    self.assertEqual(r.stdout, b'')

    def real_ui_has_hotkeys(self):
        if not os.path.exists(REAL_UI):
            return False
        r = subprocess.run([REAL_UI], capture_output=True, stdin=subprocess.DEVNULL, timeout=20)
        return b'--hotkeys' in r.stdout + r.stderr

    def test_real_upkg_ui_on_a_pty(self):
        """reachability: upkg-browse drives the real ncurses upkg-ui; Down, i installs the second add-on"""
        if not self.real_ui_has_hotkeys():
            self.skipTest('host upkg-ui missing or without --hotkeys: run "make host-upkg-ui" in upterm-ports')
        os.symlink(os.path.abspath(REAL_UI), os.path.join(self.bin, 'upkg-ui'))
        env = self.env(TERM='xterm-256color', LANG='en_US.UTF-8')
        keys = [(b'\x1bOB', 1.0),   # Down: up-git
                (b'i', 1.5),        # install; upkg-ui is gone while the command runs
                (b'\r', 1.0),       # Return at the pause: the list is back
                (b'q', 1.0)]
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                if os.path.exists(self.cmd_log):
                    os.remove(self.cmd_log)
                master, slave = pty.openpty()
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 25, 80, 0, 0))
                p = subprocess.Popen(sh + [BROWSE], stdin=slave, stdout=slave, stderr=subprocess.PIPE,
                                     env=env, start_new_session=True)
                os.close(slave)
                screen = b''
                for key, wait in keys:
                    screen = pump(master, wait, screen)
                    os.write(master, key)
                screen = pump(master, 0.5, screen)
                end = time.time() + 10
                while p.poll() is None and time.time() < end:
                    screen = pump(master, 0.1, screen)
                err = p.stderr.read().decode()
                p.stderr.close()
                os.close(master)
                if p.poll() is None:
                    p.kill()
                self.assertEqual(p.returncode, 0, err)
                self.assertEqual(self.commands(), ['install up-git'])  # the sentinel: the key reached the command
                self.assertIn(b'UP-TERM ADD-ONS', screen)
                self.assertIn(b'[OK] upkg install up-git', screen)
                self.assertIn(b'installed 2.0', screen)  # the refreshed list


if __name__ == '__main__':
    unittest.main()
