#!/usr/bin/env python3
"""The shipped lessons of dist/tutorial (shell tutorial plan L-T2): every dist/tutorial-lessons/
NN-name.lesson parses, every step (a ? line) has a check (=), a hint (~) and an answer (!),
and each answer passes its own check in a practice drawer: dist/tutorial --selftest FILE
runs the lesson with its answers and prints "ok N" or "FAIL N" per step. A check that
already passes before its answer runs (all earlier steps done) is vacuous, since any line
would pass it, and is a FAIL too. The host has no
nano, man, upkg or Version that the lessons can count on, so stubs that do nothing stand in
for them (a check reads the typed line there, never their output)."""
import os, re, shutil, subprocess, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TUTORIAL = os.path.join(ROOT, 'dist', 'tutorial')
LESSONS = os.path.join(ROOT, 'dist', 'tutorial-lessons')
VSH = os.path.join(ROOT, 'build', 'vsh_host')
FILES = sorted(f for f in os.listdir(LESSONS) if f.endswith('.lesson'))
STUBS = ('nano', 'man', 'upkg', 'Version')
# UPTerm state on the host: the window's counters in one file per key (D8: the lessons call
# ${UPTERM_STATE:-UPTerm} state KEY). "act KEY" is the user's action (the lessons' answers
# call it when UPTERM_STATE is set); "act slash NAME" is a typed slash command. UPTERM_FAKE_FROZEN
# makes every action do nothing, as a user who only pressed Return. Each state call adds the
# drawer it ran in to $UPTERM_FAKE_DIR/cwd.
FAKE_STATE = os.path.join(ROOT, 'tests', 'fake-upterm-state')  # tools/rig/tutorial_rig.py puts it on the rig too
HAVE_TMUX = shutil.which('tmux') is not None
NEEDS_TMUX = ('13-',)


class Lessons(unittest.TestCase):
    shells = [['/bin/sh']] + ([[VSH]] if os.path.exists(VSH) else [])

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix='tutlessons')
        cls.bin = os.path.join(cls.tmp, 'bin')
        os.mkdir(cls.bin)
        cls.fake = FAKE_STATE
        for name in STUBS:
            p = os.path.join(cls.bin, name)
            with open(p, 'w') as fh:
                fh.write('#!/bin/sh\nexit 0\n')
            os.chmod(p, 0o755)
        # lesson 14 asks Amarchy for its state (rx, the STATUS reply amarchy/amarchy.c builds) and counts the
        # shipped keymap rows at ENVARC:up-term/keymap, a relative path on the host: the tutorial starts in tmp
        rx = os.path.join(cls.bin, 'rx')
        with open(rx, 'w') as fh:
            fh.write('#!/bin/sh\necho "hits=0 rows=41 enabled=1 active=0"\n')
        os.chmod(rx, 0o755)
        os.mkdir(os.path.join(cls.tmp, 'ENVARC:up-term'))
        shutil.copy(os.path.join(ROOT, 'dist', 'keymap'), os.path.join(cls.tmp, 'ENVARC:up-term', 'keymap'))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp)

    def selftest(self, shell, f, frozen=False):
        """dist/tutorial --selftest on a shipped lesson: the fake window state, a private tmux socket"""
        d = tempfile.mkdtemp(dir=self.tmp)
        sock = tempfile.mkdtemp(prefix='tm')  # short: a tmux socket path is limited to ~100 characters
        self.addCleanup(shutil.rmtree, sock, True)
        env = {'PATH': self.bin + ':/usr/bin:/bin:/opt/homebrew/bin', 'HOME': self.tmp,
               'TUTORIAL_SANDBOX': os.path.join(self.tmp, 'sandbox'),
               'UPTERM_STATE': self.fake, 'UPTERM_FAKE_DIR': d, 'TMUX_TMPDIR': sock}
        if frozen:
            env['UPTERM_FAKE_FROZEN'] = '1'
        r = subprocess.run(shell + [TUTORIAL, '--selftest', os.path.join(LESSONS, f)],
                           stdin=subprocess.DEVNULL, capture_output=True, env=env,
                           cwd=self.tmp, timeout=120)
        if HAVE_TMUX:
            subprocess.run(['tmux', 'kill-server'], env=env, capture_output=True)
        self.state_dir = d
        return r.stdout.decode() + r.stderr.decode()

    def lines(self, f):
        with open(os.path.join(LESSONS, f), encoding='utf-8') as fh:
            return fh.read().split('\n')[:-1]

    def test_there_are_lessons_numbered_in_a_row(self):
        self.assertEqual([int(f.split('-')[0]) for f in FILES], list(range(1, len(FILES) + 1)))
        for f in FILES:
            self.assertRegex(f, r'^\d\d-[a-z0-9-]+\.lesson$')

    def test_every_lesson_parses_and_every_step_is_complete(self):
        for f in FILES:
            with self.subTest(lesson=f):
                with open(os.path.join(LESSONS, f), 'rb') as fh:
                    fh.read().decode('ascii')  # plain text, no emoji
                lines = self.lines(f)
                self.assertTrue(lines[0].startswith('# '), 'the first line is the title')
                steps = []
                for n, line in enumerate(lines, 1):
                    self.assertRegex(line, r'^[#>$?=~!]', '%s:%d is not a record' % (f, n))
                    if line[0] == '?':
                        steps.append({'task': line, '=': 0, '~': 0, '!': 0})
                    elif line[0] in '=~!':
                        self.assertTrue(steps, '%s:%d comes before any ? line' % (f, n))
                        steps[-1][line[0]] += 1
                self.assertTrue(steps, 'a lesson has a step')
                for s in steps:
                    self.assertGreaterEqual(s['='], 1, s['task'] + ' has no check')
                    self.assertEqual(s['~'], 1, s['task'] + ' needs one hint')
                    self.assertEqual(s['!'], 1, s['task'] + ' needs one answer')

    def test_each_answer_passes_its_own_check(self):
        for shell in self.shells:
            for f in FILES:
                with self.subTest(shell=shell[0], lesson=f):
                    if f.startswith(NEEDS_TMUX) and not HAVE_TMUX:
                        self.skipTest('%s drives tmux and this host has none' % f)
                    out = self.selftest(shell, f)
                    steps = sum(1 for l in self.lines(f) if l.startswith('?'))
                    self.assertEqual(re.findall(r'^(ok \d+|FAIL \d+)$', out, re.M),
                                     ['ok %d' % i for i in range(1, steps + 1)], out)

    def test_the_window_lesson_needs_the_actions_not_just_a_return(self):
        """lesson 12 with the actions not done (counters unchanged): every counter step fails;
        the paste step still passes on the word alone"""
        f = '12-the-up-term-window.lesson'
        for shell in self.shells:
            with self.subTest(shell=shell[0]):
                out = self.selftest(shell, f, frozen=True)
                self.assertEqual(re.findall(r'^(ok \d+|FAIL \d+)$', out, re.M),
                                 ['FAIL 1', 'FAIL 2', 'FAIL 3', 'FAIL 4', 'FAIL 5', 'ok 6', 'FAIL 7'], out)

    def test_the_window_lesson_runs_outside_the_practice_drawer(self):
        """lesson 12 opens a tab, and a new tab opens in the current drawer: in T:tutorial it kept
        the drawer in use and the tutorial could not remove it (rig 4, 2026-10-19). Every state
        check of the lesson runs in the drawer the tutorial started in."""
        f = '12-the-up-term-window.lesson'
        sandbox = os.path.realpath(os.path.join(self.tmp, 'sandbox'))
        for shell in self.shells:
            with self.subTest(shell=shell[0]):
                out = self.selftest(shell, f)
                with open(os.path.join(self.state_dir, 'cwd')) as fh:
                    dirs = [os.path.realpath(l) for l in fh.read().split()]
                self.assertTrue(dirs, out)
                self.assertEqual([x for x in dirs if x.startswith(sandbox)], [], out)

    def test_the_shipped_lessons_are_fourteen(self):
        self.assertEqual(len(FILES), 14)
        self.assertIn('12-the-up-term-window.lesson', FILES)
        self.assertIn('13-split-panes-tmux.lesson', FILES)
        self.assertIn('14-amarchy-keys.lesson', FILES)

    def test_a_check_that_passes_before_the_answer_fails(self):
        lesson = os.path.join(self.tmp, '99-vacuous.lesson')
        with open(lesson, 'w') as fh:
            fh.write('# Vacuous\n? Make a file.\n= true\n~ Try touch a.\n! touch a\n'
                     '? Make another.\n= [ -e b ]\n~ Try touch b.\n! touch b\n')
        for shell in self.shells:
            with self.subTest(shell=shell[0]):
                env = {'PATH': '/usr/bin:/bin', 'HOME': self.tmp,
                       'TUTORIAL_SANDBOX': os.path.join(self.tmp, 'sandbox')}
                r = subprocess.run(shell + [TUTORIAL, '--selftest', lesson],
                                   stdin=subprocess.DEVNULL, capture_output=True, env=env,
                                   cwd=self.tmp, timeout=60)
                out = r.stdout.decode() + r.stderr.decode()
                self.assertEqual(re.findall(r'^(ok \d+|FAIL \d+)', out, re.M), ['FAIL 1', 'ok 2'], out)


if __name__ == '__main__':
    unittest.main()
