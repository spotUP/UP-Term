#!/usr/bin/env python3
"""dist/tutorial (shell tutorial plan L-T1): the lessons run through the real script with
scripted stdin. Each case runs under the host's /bin/sh and under build/vsh_host (vsh is
/bin/sh on the Amiga). Right answers pass a step, a wrong one shows the hint, three wrong
ones show the answer; $TUTORIAL_PROGRESS gets the lesson number; a sentinel lesson
directory proves $TUTORIAL_LESSONS is the one read (dist/tutorial-lessons is the shipped
one, UP-Term:share/tutorial on the Amiga). The rig half is the plan's L-R."""
import os, pty, shutil, subprocess, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TUTORIAL = os.path.join(ROOT, 'dist', 'tutorial')
LESSONS = os.path.join(ROOT, 'dist', 'tutorial-lessons')
VSH = os.path.join(ROOT, 'build', 'vsh_host')

# a lesson of our own: its title and task text exist nowhere in the shipped lessons
SENTINEL = '''# SENTINEL-LESSON-7731
> Sentinel text.
? Make the file sentinel.txt.
= [ -f sentinel.txt ]
~ touch sentinel.txt
! touch sentinel.txt
'''
SENTINEL2 = SENTINEL.replace('7731', '7732').replace('sentinel.txt', 'second.txt')


class Tutorial(unittest.TestCase):
    shells = [['/bin/sh']] + ([[VSH]] if os.path.exists(VSH) else [])

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='tutorial')
        self.addCleanup(shutil.rmtree, self.tmp)
        self.progress = os.path.join(self.tmp, 'progress')
        self.sandbox = os.path.join(self.tmp, 'sandbox')
        self.work = os.path.join(self.tmp, 'work')
        os.mkdir(self.work)
        with open(os.path.join(self.work, 'precious.txt'), 'w') as fh:
            fh.write('mine\n')

    def lesson_dir(self, *texts):
        d = os.path.join(self.tmp, 'lessons')
        os.makedirs(d, exist_ok=True)
        for i, t in enumerate(texts, 1):
            with open(os.path.join(d, '%02d-sentinel.lesson' % i), 'w') as fh:
                fh.write(t)
        return d

    def run_tutorial(self, shell, args, typed='', lessons=LESSONS):
        env = {'PATH': '/usr/bin:/bin', 'HOME': self.tmp, 'TUTORIAL_LESSONS': lessons,
               'TUTORIAL_PROGRESS': self.progress, 'TUTORIAL_SANDBOX': self.sandbox}
        r = subprocess.run(shell + [TUTORIAL] + args, input=typed.encode(), cwd=self.work,
                           capture_output=True, env=env, timeout=60)
        return r.returncode, r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')

    def done(self):
        if not os.path.exists(self.progress):
            return []
        with open(self.progress) as fh:
            return fh.read().split()

    def each_shell(self, fn):
        for shell in self.shells:
            with self.subTest(shell=shell[0]):
                if os.path.exists(self.progress):
                    os.remove(self.progress)
                fn(shell)

    def test_right_answers_finish_lesson_one(self):
        def case(shell):
            rc, out = self.run_tutorial(shell, ['1'], 'ls\npwd\ncd docs\ncd ..\n')
            self.assertEqual(rc, 0, out)
            self.assertEqual(out.count('Right.'), 4, out)
            self.assertIn('Lesson finished.', out)
            self.assertNotIn('Not quite', out)
            self.assertEqual(self.done(), ['1'])
        self.each_shell(case)

    def test_a_drawer_still_in_use_is_named_at_the_end(self):
        """a tab opened in the practice drawer keeps it as its current drawer; rm -rf then left
        T:tutorial behind without a word (rig 4, 2026-10-19). A drawer that cannot go is named.
        Here a write-protected sub drawer stands in for the drawer in use."""
        held = SENTINEL.replace('? Make', '$ mkdir keep; touch keep/f; chmod 555 keep\n? Make')
        lessons = self.lesson_dir(held)
        def case(shell):
            try:
                rc, out = self.run_tutorial(shell, ['1'], 'touch sentinel.txt\n', lessons)
                self.assertIn('Lesson finished.', out)
                self.assertTrue(os.path.exists(self.sandbox), out)
                self.assertIn('is still in use', out)
            finally:
                if os.path.exists(os.path.join(self.sandbox, 'keep')):
                    os.chmod(os.path.join(self.sandbox, 'keep'), 0o755)
                shutil.rmtree(self.sandbox, True)
        self.each_shell(case)

    def test_home_that_is_a_volume_keeps_the_progress_file_in_it(self):
        """HOME is "System:" on the Amiga: "$HOME/.tutorial" would be "System:/.tutorial", the volume's parent,
        which grep cannot open, so `tutorial next` never saw a finished lesson (rig 4, 2026-10-19)."""
        vol = os.path.join(self.tmp, 'vol:')
        os.mkdir(vol)
        env = {'PATH': '/usr/bin:/bin', 'HOME': vol, 'TUTORIAL_LESSONS': LESSONS, 'TUTORIAL_SANDBOX': self.sandbox}
        for shell in self.shells:
            with self.subTest(shell=shell[0]):
                for f in (vol + '.tutorial', os.path.join(vol, '.tutorial')):
                    if os.path.exists(f):
                        os.remove(f)
                subprocess.run(shell + [TUTORIAL, '1'], input=b'ls\npwd\ncd docs\ncd ..\n', cwd=self.work,
                               capture_output=True, env=env, timeout=60)
                self.assertTrue(os.path.exists(vol + '.tutorial'))
                self.assertFalse(os.path.exists(os.path.join(vol, '.tutorial')))
                r = subprocess.run(shell + [TUTORIAL, 'next'], input=b'', cwd=self.work, capture_output=True, env=env,
                                   timeout=60)
                self.assertIn('=== Looking at files ===', r.stdout.decode(errors='replace'))   # lesson 1 is done: next is lesson 2

    def test_wrong_answer_shows_the_hint_then_the_right_one_passes(self):
        def case(shell):
            rc, out = self.run_tutorial(shell, ['1'], 'echo a\nls\n')
            self.assertIn('Not quite. Hint: Type ls and press Return.', out)
            self.assertEqual(out.count('Right.'), 1, out)
            self.assertNotIn('The answer is', out)
        self.each_shell(case)

    def test_three_wrong_answers_show_and_run_the_answer(self):
        def case(shell):
            rc, out = self.run_tutorial(shell, ['1'], 'echo a\necho b\necho c\npwd\n')
            self.assertEqual(out.count('Not quite.'), 3, out)
            self.assertIn('The answer is:  ls', out)
            # the answer ran in the window: docs, the setup's drawer, is listed
            self.assertRegex(out, r'The answer is:  ls\n(.*\n)*?docs\n', out)
            self.assertIn('Right.', out)  # the next step (pwd) went on
        self.each_shell(case)

    def test_the_tutorial_lists_the_window_and_tmux_lessons(self):
        def case(shell):
            rc, out = self.run_tutorial(shell, [])
            self.assertIn('  [ ] 12  The UP-Term window', out)
            self.assertIn('  [ ] 13  Split panes with tmux', out)
            self.assertIn('  [ ] 14  Amarchy: Super-key shortcuts', out)
            self.assertEqual(out.count('  [ ] '), 14, out)
        self.each_shell(case)

    def test_learn_word_shows_in_the_task_and_the_text(self):
        # a setup line sets $LEARN_WORD; @WORD@ in the text and the task is that word (copy and paste step)
        lesson = ('# WORD-LESSON-4417\n> Text with @WORD@ inside.\n$ LEARN_WORD=zq7wx\n'
                  '? Copy @WORD@ and paste it.\n= [ "$LEARN_LINE" = "$LEARN_WORD" ]\n~ Paste it.\n! echo zq7wx\n')
        d = self.lesson_dir(lesson)
        def case(shell):
            rc, out = self.run_tutorial(shell, ['1'], 'zq7wx\n', lessons=d)
            self.assertIn('Text with zq7wx inside.', out)
            self.assertIn('Task: Copy zq7wx and paste it.', out)
            self.assertNotIn('@WORD@', out)
        self.each_shell(case)

    def test_empty_line_runs_the_checks_for_an_action_in_the_window(self):
        # a slash command reaches read as an empty line; the step's check sees what it did
        window = ('# WINDOW-LESSON-5519\n? Do it in the window, then press Return.\n'
                  '= [ -f "$HOME/window-flag" ]\n! true\n')
        d = self.lesson_dir(window, SENTINEL)
        def case(shell):
            flag = os.path.join(self.tmp, 'window-flag')
            if os.path.exists(flag):
                os.remove(flag)
            # nothing done in the window yet: empty lines ask again and count no miss
            rc, out = self.run_tutorial(shell, ['1'], '\n\n\n', lessons=d)
            self.assertNotIn('Right.', out)
            self.assertNotIn('Not quite', out)
            self.assertNotIn('The answer is', out)
            open(flag, 'w').close()
            rc, out = self.run_tutorial(shell, ['1'], '\n', lessons=d)
            self.assertEqual(out.count('Right.'), 1, out)
            self.assertIn('Lesson finished.', out)
            # a plain step is not passed by an empty line
            rc, out = self.run_tutorial(shell, ['2'], '\n\n\n', lessons=d)
            self.assertNotIn('Right.', out)
            self.assertNotIn('Not quite', out)
        self.each_shell(case)

    def test_colon_words_hint_skip_quit(self):
        def case(shell):
            rc, out = self.run_tutorial(shell, ['1'], ':hint\n:skip\n:quit\n')
            self.assertIn('Hint: Type ls and press Return.', out)
            self.assertIn('Skipped.', out)
            self.assertIn('Progress kept', out)
            self.assertNotIn('Lesson finished.', out)
            self.assertEqual(self.done(), [])
        self.each_shell(case)

    def test_end_of_input_keeps_progress_and_cleans_up(self):
        def case(shell):
            rc, out = self.run_tutorial(shell, ['1'], 'ls\n')
            self.assertEqual(rc, 0, out)
            self.assertIn('End of input', out)
            self.assertEqual(self.done(), [])
            self.assertFalse(os.path.exists(self.sandbox), 'the practice drawer is removed')
        self.each_shell(case)

    def test_sentinel_lesson_dir_is_the_one_read(self):
        def case(shell):
            d = self.lesson_dir(SENTINEL)
            rc, out = self.run_tutorial(shell, ['1'], 'touch sentinel.txt\n', lessons=d)
            self.assertIn('SENTINEL-LESSON-7731', out)
            self.assertIn('Make the file sentinel.txt.', out)
            self.assertNotIn('Print the list of files here', out)
            self.assertEqual(self.done(), ['1'])
            self.assertFalse(os.path.exists(os.path.join(self.work, 'sentinel.txt')),
                             'the lesson ran in the practice drawer')
        self.each_shell(case)

    def test_menu_marks_finished_and_next_picks_the_first_open(self):
        def case(shell):
            d = self.lesson_dir(SENTINEL, SENTINEL2)
            with open(self.progress, 'w') as fh:
                fh.write('1\n')
            rc, out = self.run_tutorial(shell, [], lessons=d)
            self.assertRegex(out, r'\[x\] 1  SENTINEL-LESSON-7731')
            self.assertRegex(out, r'\[ \] 2  SENTINEL-LESSON-7732')
            rc, out = self.run_tutorial(shell, ['next'], 'touch second.txt\n', lessons=d)
            self.assertIn('SENTINEL-LESSON-7732', out)
            self.assertNotIn('SENTINEL-LESSON-7731', out)
            self.assertEqual(self.done(), ['1', '2'])
            rc, out = self.run_tutorial(shell, ['next'], lessons=d)
            self.assertIn('All lessons are finished', out)
        self.each_shell(case)

    def test_next_with_all_finished_lists_and_asks_on_a_terminal_only(self):
        """the tab runs `tutorial next`: on a terminal, all finished shows the list and asks; Return closes,
        a number repeats that lesson. Not a terminal: today's one-line message, no prompt."""
        import pty, select, time
        def on_terminal(shell, d, typed):
            m, sl = pty.openpty()
            env = {'PATH': '/usr/bin:/bin', 'HOME': self.tmp, 'TUTORIAL_LESSONS': d,
                   'TUTORIAL_PROGRESS': self.progress, 'TUTORIAL_SANDBOX': self.sandbox}
            pr = subprocess.Popen(shell + [TUTORIAL, 'next'], stdin=sl, stdout=sl, stderr=sl, cwd=self.work, env=env)
            os.close(sl)
            os.write(m, typed.encode())
            out = b''
            end = time.time() + 30
            while time.time() < end:
                if select.select([m], [], [], 0.2)[0]:
                    try:
                        c = os.read(m, 4096)
                    except OSError:
                        break
                    if not c:
                        break
                    out += c
                elif pr.poll() is not None:
                    break
            if pr.poll() is None:
                pr.kill()
            pr.wait(timeout=60)
            os.close(m)
            return out.decode(errors='replace')
        def case(shell):
            d = self.lesson_dir(SENTINEL)
            with open(self.progress, 'w') as fh:
                fh.write('1\n')
            rc, out = self.run_tutorial(shell, ['next'], lessons=d)
            self.assertIn('All lessons are finished', out)
            self.assertNotIn('Return to close', out)
            out = on_terminal(shell, d, '\n')
            self.assertIn('SENTINEL-LESSON-7731', out)
            self.assertIn('Lesson number, or Return to close: ', out)
            out = on_terminal(shell, d, '1\ntouch sentinel.txt\n')
            self.assertIn('Lesson finished', out)
        self.each_shell(case)

    def test_reset_forgets_progress(self):
        def case(shell):
            with open(self.progress, 'w') as fh:
                fh.write('1\n')
            self.run_tutorial(shell, ['reset'])
            self.assertEqual(self.done(), [])
        self.each_shell(case)

    def test_rm_in_a_lesson_never_touches_the_users_files(self):
        def case(shell):
            d = self.lesson_dir(SENTINEL)
            self.run_tutorial(shell, ['1'], 'rm -rf *\ntouch sentinel.txt\n', lessons=d)
            self.assertTrue(os.path.exists(os.path.join(self.work, 'precious.txt')))
        self.each_shell(case)

    def test_bad_arguments(self):
        def case(shell):
            rc, out = self.run_tutorial(shell, ['99'])
            self.assertNotEqual(rc, 0)
            self.assertIn('no lesson 99', out)
            rc, out = self.run_tutorial(shell, ['bogus'])
            self.assertEqual(rc, 2)
            self.assertIn('usage: tutorial', out)
        self.each_shell(case)




VSHRC = os.path.join(ROOT, 'dist', 'vshrc')
HINT = 'New to a Unix shell? Type  tutorial'


class FirstRunHint(unittest.TestCase):
    """plan L-8: an interactive shell on a terminal that sources dist/vshrc prints the one-line hint
    until $HOME/.tutorial-hint-off exists or lesson 1 is in the progress file. The shell runs on a
    pty (the hint needs [ -t 1 ]) as `sh -i`, the way vsh sources the file at an interactive start."""

    def setUp(self):
        self.home = tempfile.mkdtemp(prefix='hint')
        self.addCleanup(shutil.rmtree, self.home)

    def startup(self, interactive=True, tty=True):
        env = {'PATH': '/usr/bin:/bin', 'HOME': self.home, 'TERM': 'vt100'}
        cmd = ['/bin/sh'] + (['-i'] if interactive else []) + ['-c', '. ' + VSHRC]
        if not tty:
            r = subprocess.run(cmd, capture_output=True, env=env, timeout=30, stdin=subprocess.DEVNULL)
            return r.stdout.decode(errors='replace')
        out = []
        pid, fd = pty.fork()
        if pid == 0:
            os.execve(cmd[0], cmd, env)
        while True:
            try:
                data = os.read(fd, 4096)
            except OSError:
                break
            if not data:
                break
            out.append(data)
        os.waitpid(pid, 0)
        return b''.join(out).decode(errors='replace')

    def test_hint_shows_then_goes_with_the_off_file_and_with_lesson_one(self):
        self.assertIn(HINT, self.startup())
        self.assertIn('.tutorial-hint-off', self.startup())  # it says how to hide itself

    def test_hint_lines_fit_an_80_column_window(self):
        lines = [l for l in self.startup().splitlines() if 'tutorial' in l]
        self.assertTrue(lines)
        for line in lines:
            self.assertLessEqual(len(line.strip()), 79, line)  # longer wraps mid-word in the default UP-Term window
        with open(os.path.join(self.home, '.tutorial'), 'w') as fh:
            fh.write('2\n')
        self.assertIn(HINT, self.startup())  # lesson 2 done, lesson 1 is not
        with open(os.path.join(self.home, '.tutorial'), 'a') as fh:
            fh.write('1\n')
        self.assertNotIn(HINT, self.startup())
        os.remove(os.path.join(self.home, '.tutorial'))
        self.assertIn(HINT, self.startup())
        open(os.path.join(self.home, '.tutorial-hint-off'), 'w').close()
        self.assertNotIn(HINT, self.startup())

    def test_no_hint_for_a_script_or_a_pipe(self):
        self.assertNotIn(HINT, self.startup(interactive=False))
        self.assertNotIn(HINT, self.startup(tty=False))


if __name__ == '__main__':
    unittest.main()
