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


class Lessons(unittest.TestCase):
    shells = [['/bin/sh']] + ([[VSH]] if os.path.exists(VSH) else [])

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix='tutlessons')
        cls.bin = os.path.join(cls.tmp, 'bin')
        os.mkdir(cls.bin)
        for name in STUBS:
            p = os.path.join(cls.bin, name)
            with open(p, 'w') as fh:
                fh.write('#!/bin/sh\nexit 0\n')
            os.chmod(p, 0o755)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp)

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
                    env = {'PATH': self.bin + ':/usr/bin:/bin', 'HOME': self.tmp,
                           'TUTORIAL_SANDBOX': os.path.join(self.tmp, 'sandbox')}
                    r = subprocess.run(shell + [TUTORIAL, '--selftest', os.path.join(LESSONS, f)],
                                       stdin=subprocess.DEVNULL, capture_output=True, env=env,
                                       cwd=self.tmp, timeout=60)
                    out = r.stdout.decode() + r.stderr.decode()
                    steps = sum(1 for l in self.lines(f) if l.startswith('?'))
                    self.assertEqual(re.findall(r'^(ok \d+|FAIL \d+)$', out, re.M),
                                     ['ok %d' % i for i in range(1, steps + 1)], out)

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
