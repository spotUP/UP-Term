#!/usr/bin/env python3
"""dist/upkg-browse (plan 2026-10-21-upkg-browse, Phase B): the "upkg browse" list loop.
A fake upkg (list, list-installed, list-upgradable, install, remove, upgrade, info over state files)
and a fake upkg-ui sit on the PATH. The fake upkg-ui reads "RC VALUE" lines from its stdin, writes
VALUE to stderr, exits RC and logs its argv; the keyboard is a pty with those lines (and the Return
of each pause) typed in. Each case runs under /bin/sh and under build/vsh_host when it is built.
One case runs the REAL host upkg-ui (upterm-ports build/host/upkg-ui) on a pty with keys sent; it
is skipped when that binary is missing or has no --hotkeys."""
import os, re, pty, select, shutil, struct, fcntl, termios, signal, subprocess, tempfile, time, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BROWSE = os.path.join(ROOT, 'dist', 'upkg-browse')
REAL_UI = os.path.join(ROOT, '..', 'upterm-ports', 'build', 'host', 'upkg-ui', 'upkg-ui')
VSH = os.path.join(ROOT, 'build', 'vsh_host')
HEAD = '--title UP-TERM ADD-ONS --backtitle 3 add-ons --colors --hotkeys iruq '
PROMPT = 'Enter info   i install   r remove   u upgrade   q quit'
ROWS = ('up-vim \\Z2installed 1.3   Vim, the text editor '
        'up-git available 2.0   Git - the stupid content tracker '
        'up-perl \\Z3upgrade 5.38 to 5.40   Perl language')

FAKE_UI = '''#!/bin/sh
printf '%s\\n' "$*" >> "$UI_LOG"
for a in "$@"; do
    case $prev in --textbox) cp "$a" "$UI_LOG.text" ;; esac
    case $a in --programbox) cat >> "$PB_LOG"; exit 0 ;; esac
    prev=$a
done
IFS=' ' read -r rc value
printf '%s' "$value" >&2
exit "$rc"
'''

# ncurses clear: logged, never drawn
FAKE_CLEAR = '''#!/bin/sh
echo clear >> "${CLEAR_LOG:-/dev/null}"
'''

FAKE_UPKG = '''#!/bin/sh
case $1 in list|list-installed|list-upgradable|list-browse) echo "$1" >> "$LIST_LOG" ;; esac
case $1 in
list-browse) cat "$STATE/feed" > "$2.all"; cat "$STATE/installed" > "$2.inst"
    cat "$STATE/upgradable" > "$2.upg"; cat "$STATE/new" > "$2.new"; exit 0 ;;
list) cat "$STATE/feed"; exit 0 ;;
list-installed) cat "$STATE/installed"; exit 0 ;;
list-upgradable) cat "$STATE/upgradable"; exit 0 ;;
esac
echo "$*" >> "$CMD_LOG"
case $1 in
install) echo "installing $2"; echo "a warning" >&2
    [ -n "$INSTALL_RC" ] && exit "$INSTALL_RC"
    grep "^$2 - " "$STATE/feed" | sed 's/ - [^-]*$//' >> "$STATE/installed" ;;
remove) grep -v "^$2 - " "$STATE/installed" > "$STATE/x"; mv "$STATE/x" "$STATE/installed" ;;
upgrade) grep -v "^$2 - " "$STATE/installed" > "$STATE/x"; mv "$STATE/x" "$STATE/installed"
    echo "$2 - 5.40" >> "$STATE/installed"; : > "$STATE/upgradable" ;;
info) echo "info of $2"; echo "Description: line two" ;;
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


def back_to_the_shell_after_drawing(stream):
    """True when the terminal leaves the alternate screen (rmcup, \\E[?1049l) after text was drawn on it:
    the Shell window shows between two upkg-ui screens"""
    for part in stream.split(b'\x1b[?1049h')[1:]:
        drawn = part.split(b'\x1b[?1049l')[0] if b'\x1b[?1049l' in part else b''
        if re.sub(rb'\x1b(\[[0-9;?>=]*[ -/]*[@-~]|[()][0-9A-Za-z]|[@-_])', b'', drawn).strip():
            return True
    return False


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
        self.list_log = os.path.join(self.tmp, 'list.log')
        self.pb_log = os.path.join(self.tmp, 'pb.log')
        self.clear_log = os.path.join(self.tmp, 'clear.log')
        with open(os.path.join(self.bin, 'clear'), 'w') as f:
            f.write(FAKE_CLEAR)
        os.chmod(os.path.join(self.bin, 'clear'), 0o755)
        self.lists_dir = os.path.join(self.tmp, 'lists')
        os.mkdir(self.lists_dir)
        self.upkg = os.path.join(self.bin, 'upkg')
        self.fill('feed', 'up-vim - 1.3 - Vim, the text editor\n'
                          'up-git - 2.0 - Git - the stupid content tracker\n'
                          'up-perl - 5.40 - Perl language\n')
        self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
        self.fill('upgradable', 'up-perl - 5.38 - 5.40\n')
        self.fill('new', '')
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
               'UI_LOG': self.ui_log, 'CMD_LOG': self.cmd_log, 'LIST_LOG': self.list_log, 'PB_LOG': self.pb_log, 'CLEAR_LOG': self.clear_log, 'UPKG_LISTS_DIR': self.lists_dir, 'HOME': self.tmp, 'TMPDIR': self.tmp}
        env.update(extra)
        return env

    def run_browse(self, shell, typed=None, args=(), rc=0, **extra):
        """run on a pty with TYPED waiting on the keyboard (None: stdin is /dev/null)"""
        for f in (self.ui_log, self.cmd_log, self.list_log, self.pb_log, self.clear_log, self.ui_log + '.text'):
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

    def programbox(self):
        return self.read(self.pb_log)

    def list_reads(self):
        return self.read(self.list_log)

    def clears(self):
        return len(self.read(self.clear_log))

    def test_rows_and_markers_split_at_the_first_and_second_separator(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_browse(sh, "0 q up-vim\n")
                self.assertEqual(self.calls(), [HEAD + '--menu ' + PROMPT + ' 0 0 0 ' + ROWS])
                self.assertEqual(self.commands(), [])

    def test_install_runs_in_a_programbox_keeps_the_cursor_and_refreshes_the_marker(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                r = self.run_browse(sh, "0 i up-git\n0 q up-git\n")
                self.assertEqual(self.commands(), ['install up-git'])
                calls = self.calls()
                self.assertEqual(len(calls), 3)
                self.assertEqual(calls[1], '--title UP-TERM ADD-ONS --programbox upkg install up-git 0 0')
                self.assertEqual(self.programbox(), ['installing up-git', 'a warning', '[OK] upkg install up-git'])
                self.assertEqual(calls[2], HEAD + '--default-item up-git --menu ' + PROMPT + ' 0 0 0 '
                                 + ROWS.replace('up-git available 2.0', 'up-git \\Z2installed 2.0'))
                # nothing reached the Shell window: no output and no "Press Return"
                self.assertEqual((r.stdout, r.stderr), (b'', b''))

    def test_a_failed_command_ends_the_programbox_with_error_and_its_code(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_browse(sh, "0 i up-git\n0 q up-git\n", INSTALL_RC='3')
                self.assertEqual(self.programbox()[-1], '[ERROR] upkg install up-git (exit 3)')
                self.assertIn('up-git available 2.0', self.calls()[2])
                self.assertEqual((r.stdout, r.stderr), (b'', b''))

    def test_a_package_with_the_interactive_control_field_runs_in_the_plain_window(self):
        # UP-Term-Interactive is read from the feed's Packages list: opkg's "upkg info" drops unknown fields
        self.fake_ui()
        with open(os.path.join(self.lists_dir, 'up-feed'), 'w') as f:
            f.write('Package: up-vim\nVersion: 1.3\n\nPackage: up-git\nVersion: 2.0\nUP-Term-Interactive: yes\n\n')
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')  # the first shell installed it
                r = self.run_browse(sh, "0 i up-git\n\n0 q up-git\n")
                self.assertEqual(self.commands(), ['install up-git'])
                self.assertEqual(self.programbox(), [])
                self.assertEqual(len(self.calls()), 2)
                # the plain window starts clean, not under the list upkg-ui left; quit clears again
                self.assertEqual(self.clears(), 2)
                self.assertIn(b'installing up-git', r.stdout)
                self.assertIn(b'[OK] upkg install up-git', r.stdout)
        # the field belongs to up-git only: another package still gets the box
        self.run_browse(self.shells[0], "0 u up-perl\n0 q up-perl\n")
        self.assertEqual(self.programbox(), ['[OK] upkg upgrade up-perl'])

    def test_enter_shows_info_with_only_the_buttons_that_apply(self):
        self.fake_ui()
        for row, want in (('up-git', 'Install,Back'), ('up-vim', 'Remove,Back'), ('up-perl', 'Upgrade,Remove,Back')):
            for sh in self.shells:
                with self.subTest(sh=sh[0], row=row):
                    r = self.run_browse(sh, "0 enter %s\n0 Back\n0 q %s\n" % (row, row))
                    calls = self.calls()
                    self.assertEqual(calls[1], '--title UP-TERM ADD-ONS --buttons %s --textbox %s 0 0' % (
                        want, calls[1].split('--textbox ')[1].split(' 0 0')[0]))
                    self.assertEqual(self.read(self.ui_log + '.text'), ['info of ' + row, 'Description: line two'])
                    self.assertEqual(self.commands(), ['info ' + row])
                    self.assertIn('--default-item ' + row, calls[2])
                    self.assertEqual((r.stdout, r.stderr), (b'', b''))

    def test_a_button_of_the_info_box_runs_its_action(self):
        self.fake_ui()
        cases = [('up-git', 'Install', 'install up-git', ''), ('up-perl', 'Upgrade', 'upgrade up-perl', ''),
                 ('up-vim', 'Remove', 'remove up-vim', '0\n')]  # Remove asks the yes/no question first
        for row, button, cmd, yes in cases:
            for sh in self.shells:
                with self.subTest(sh=sh[0], button=button):
                    self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                    self.fill('upgradable', 'up-perl - 5.38 - 5.40\n')
                    self.run_browse(sh, "0 enter %s\n0 %s\n%s0 q %s\n" % (row, button, yes, row))
                    self.assertEqual(self.commands(), ['info ' + row, cmd])
                    self.assertEqual(self.programbox()[-1], '[OK] upkg ' + cmd)

    def test_escape_in_the_info_box_goes_back_to_the_list(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_browse(sh, "0 enter up-git\n255\n0 q up-git\n")
                self.assertEqual(self.commands(), ['info up-git'])
                self.assertIn('--default-item up-git', self.calls()[2])

    def test_a_new_row_says_new_in_cyan(self):
        self.fake_ui()
        self.fill('new', 'up-git - 2.0 - Git - the stupid content tracker\n')
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.run_browse(sh, "0 q up-git\n")
                self.assertEqual(self.calls(), [HEAD + '--menu ' + PROMPT + ' 0 0 0 '
                                                + ROWS.replace('up-git available 2.0', 'up-git \\Z6new 2.0')])

    def test_remove_asks_first_and_yes_removes(self):
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                r = self.run_browse(sh, "0 r up-vim\n0\n0 q up-vim\n")
                calls = self.calls()
                self.assertEqual(calls[1], '--title UP-TERM ADD-ONS --defaultno --yesno Remove up-vim? 0 0')
                self.assertEqual(self.commands(), ['remove up-vim'])
                self.assertEqual(self.programbox()[-1], '[OK] upkg remove up-vim')
                self.assertIn('up-vim available 1.3', calls[3])
                self.assertEqual((r.stdout, r.stderr), (b'', b''))

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
                self.run_browse(sh, "0 u up-perl\n0 q up-perl\n")
                self.assertEqual(self.commands(), ['upgrade up-perl'])
                self.assertIn('up-perl \\Z2installed 5.40   Perl language', self.calls()[2])

    def test_the_lists_are_read_again_only_after_a_command_that_changes_the_add_ons(self):
        # each upkg list run takes seconds on a 68k: info and a refused key go back to the same lists
        self.fake_ui()
        once = ['list-browse']
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                self.run_browse(sh, "0 enter up-vim\n0 Back\n0 i up-vim\n0\n0 q up-vim\n")
                self.assertEqual(self.list_reads(), once)
                self.run_browse(sh, "0 i up-git\n0 q up-git\n")
                self.assertEqual(self.list_reads(), once * 2)

    def test_the_lists_upkg_browse_wrote_start_the_list_without_a_upkg_run(self):
        # upkg browse hands over the three lists it wrote (UPKG_BROWSE_LISTS); the script runs upkg
        # again only after a command that changed the add-ons, and leaves the handed files to upkg.
        # Its .inst rows carry the feed's descriptions (one load holds the feeds), where a plain
        # list-installed shows none on an Amiga: the version is still read up to the first " - ".
        self.fake_ui()
        given = os.path.join(self.tmp, 'upkg-lists.1')
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                self.fill('installed', 'up-vim - 1.3\nup-perl - 5.38\n')
                for ext, name in (('all', 'feed'), ('upg', 'upgradable')):
                    shutil.copy(os.path.join(self.state, name), given + '.' + ext)
                with open(given + '.inst', 'w') as f:
                    f.write('up-perl - 5.38 - Perl language\nup-vim - 1.3 - Vim, the text editor\n')
                self.run_browse(sh, "0 i up-git\n0 q up-git\n", UPKG_BROWSE_LISTS=given)
                self.assertEqual(self.list_reads(), ['list-browse'])
                self.assertEqual(self.calls()[0], HEAD + '--menu ' + PROMPT + ' 0 0 0 ' + ROWS)
                self.assertIn('up-git \\Z2installed 2.0', self.calls()[2])
                self.assertTrue(os.path.exists(given + '.upg'))
                self.assertEqual([f for f in os.listdir(self.tmp) if f.startswith('upkg-browse')], [])

    def test_a_key_that_does_not_fit_the_row_shows_a_msgbox_and_runs_nothing(self):
        self.fake_ui()
        cases = [("0 i up-vim\n0\n0 q up-vim\n", 'up-vim is already installed.'),
                 ("0 r up-git\n0\n0 q up-git\n", 'up-git is not installed.'),
                 ("0 u up-vim\n0\n0 q up-vim\n", 'up-vim has no newer version.')]
        for sh in self.shells:
            for typed, line in cases:
                with self.subTest(sh=sh[0], line=line):
                    r = self.run_browse(sh, typed)
                    self.assertEqual(self.calls()[1], '--title UP-TERM ADD-ONS --msgbox %s 0 0' % line)
                    self.assertEqual(self.commands(), [])
                    self.assertEqual(len(self.calls()), 3)  # no yes/no question either
                    self.assertEqual((r.stdout, r.stderr), (b'', b''))

    def test_quit_and_escape_leave_with_zero(self):
        self.fake_ui()
        for sh in self.shells:
            for typed in ("0 q up-vim\n", "255\n", "1\n"):
                with self.subTest(sh=sh[0], typed=typed):
                    self.run_browse(sh, typed)
                    self.assertEqual(len(self.calls()), 1)
                    self.assertEqual(self.commands(), [])

    def test_leaving_clears_the_last_screen(self):
        """regression (owner, 2026-10-21): upkg-ui stays on the normal screen between screens (no Shell
        flash), so the list it drew last is cleared on the way out, and an upkg-ui failure is not"""
        self.fake_ui()
        for sh in self.shells:
            for typed, rc, n in (("0 q up-vim\n", 0, 1), ("255\n", 0, 1), ("1\n", 0, 1), ("2 usage\n", 2, 0)):
                with self.subTest(sh=sh[0], typed=typed):
                    self.run_browse(sh, typed, rc=rc)
                    self.assertEqual(self.clears(), n)

    def test_an_upkg_ui_failure_is_reported(self):
        """regression (rig 2, 2026-10-21): an upkg-ui built before --hotkeys exits 2 with its usage on
        stderr, which the browse script sends to its answer file; the script then left with 0 and no word"""
        self.fake_ui()
        for sh in self.shells:
            with self.subTest(sh=sh[0]):
                r = self.run_browse(sh, "2 usage: upkg-ui\n", rc=2)
                self.assertTrue(r.stderr.startswith(b'usage: upkg-ui'), r.stderr)
                self.assertTrue(r.stderr.endswith(b'upkg browse: upkg-ui failed (exit 2).\n'), r.stderr)
                self.assertEqual(r.returncode, 2)
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
        return b'--hotkeys' in r.stdout + r.stderr and b'--programbox' in r.stdout + r.stderr

    def test_real_upkg_ui_on_a_pty(self):
        """reachability: upkg-browse drives the real ncurses upkg-ui; Down, i installs the second add-on in a programbox"""
        if not self.real_ui_has_hotkeys():
            self.skipTest('host upkg-ui missing or without --hotkeys: run "make host-upkg-ui" in upterm-ports')
        os.symlink(os.path.abspath(REAL_UI), os.path.join(self.bin, 'upkg-ui'))
        env = self.env(TERM='xterm-256color', LANG='en_US.UTF-8')
        keys = [(b'\x1bOB', 1.0),   # Down: up-git
                (b'i', 1.5),        # install; upkg-ui is gone while the command runs
                (b'\r', 1.0),       # Return on the programbox OK: the list is back
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
                    if key == b'\r':  # upkg-ui drops keys typed while the command runs: wait for its last line
                        end = time.time() + 60
                        while b'[OK] upkg install up-git' not in screen and time.time() < end:
                            screen = pump(master, 0.2, screen)
                        screen = pump(master, 0.5, screen)
                    os.write(master, key)
                screen = pump(master, 0.5, screen)
                end = time.time() + 10
                while p.poll() is None and time.time() < end:
                    screen = pump(master, 0.1, screen)
                if p.poll() is None:
                    os.killpg(p.pid, signal.SIGKILL)  # the session: upkg-ui too, or the stderr read below never ends
                err = p.stderr.read().decode()
                p.stderr.close()
                os.close(master)
                self.assertEqual(p.returncode, 0, err)
                self.assertEqual(self.commands(), ['install up-git'])  # the sentinel: the key reached the command
                self.assertIn(b'UP-TERM ADD-ONS', screen)
                self.assertIn(b'[OK] upkg install up-git', screen)
                self.assertIn(b'installed 2.0', screen)  # the refreshed list
                # regression (owner, 2026-10-21, "Shell flashes between dialogs"): list, programbox and
                # list again never switch back to the Shell's screen in between
                self.assertGreaterEqual(screen.count(b'\x1b[?1049h'), 3)
                self.assertFalse(back_to_the_shell_after_drawing(screen))


if __name__ == '__main__':
    unittest.main()
