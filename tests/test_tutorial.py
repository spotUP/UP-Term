#!/usr/bin/env python3
"""dist/tutorial (shell tutorial plan L-T1): the lessons run through the real script with
scripted stdin. Each case runs under the host's /bin/sh and under build/vsh_host (vsh is
/bin/sh on the Amiga). Right answers pass a step, a wrong one shows the hint, three wrong
ones show the answer; $TUTORIAL_PROGRESS gets the lesson number; a sentinel lesson
directory proves $TUTORIAL_LESSONS is the one read (dist/tutorial-lessons is the shipped
one, UP-Term:share/tutorial on the Amiga). The rig half is the plan's L-R."""
import os, shutil, subprocess, tempfile, unittest

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


if __name__ == '__main__':
    unittest.main()
