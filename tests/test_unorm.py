#!/usr/bin/env python3
"""claude/unorm.c's NFKC (amiga-pi's edit, as pi's normalize("NFKC")) against
Python's unicodedata 16.0.0, the Unicode version of node 24 that pi runs on:
every code point alone, then random sequences of starters, combining marks
(every class) and Hangul jamo, so ordering and composition are covered.
Runs build/unorm_drv (make build/unorm_drv)."""
import os, random, subprocess, sys, unicodedata as u, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DRV = os.path.join(ROOT, 'build', 'unorm_drv')


def cps():
    return [c for c in range(0x110000) if not 0xD800 <= c < 0xE000 and c != 0x0A]


def run(seqs):
    data = ''.join(' '.join('%X' % c for c in s) + '\n' for s in seqs)
    out = subprocess.run([DRV], input=data.encode(), stdout=subprocess.PIPE, check=True).stdout.decode()
    return [[int(x, 16) for x in l.split()] for l in out.split('\n')[:-1]]


def want(s):
    return [ord(c) for c in u.normalize('NFKC', ''.join(map(chr, s)))]


class Unorm(unittest.TestCase):
    def test_version(self):
        self.assertEqual(u.unidata_version, '16.0.0')

    def test_every_code_point(self):
        seqs = [[c] for c in cps() if c]
        got = run(seqs)
        bad = [(s, g, want(s)) for s, g in zip(seqs, got) if g != want(s)]
        self.assertEqual(bad[:5], [], '%d code points differ' % len(bad))

    def test_random_sequences(self):
        rnd = random.Random(1611)
        marks = [c for c in cps() if u.combining(chr(c))]
        starters = [c for c in range(0x20, 0x250)] + list(range(0x1100, 0x1176)) + \
            list(range(0x11A8, 0x11C3)) + [0xAC00, 0xAC01, 0xD7A3, 0x3099, 0x309A, 0x304B, 0x30AB,
                                           0x0915, 0x093C, 0x1D157, 0x1D165, 0x0CC6, 0x0CC2, 0x0CD5]
        comp = [c for c in range(0xC0, 0x1F00) if u.decomposition(chr(c)) and
                not u.decomposition(chr(c)).startswith('<')]
        seqs = []
        for _ in range(60000):
            s = []
            for _ in range(rnd.randint(1, 7)):
                k = rnd.random()
                s.append(rnd.choice(marks if k < 0.45 else comp if k < 0.6 else starters))
            seqs.append(s)
        got = run(seqs)
        bad = [(s, g, want(s)) for s, g in zip(seqs, got) if g != want(s)]
        self.assertEqual(bad[:5], [], '%d sequences differ' % len(bad))

    def test_not_utf8_is_replacement(self):
        # a byte that is not UTF-8 counts as U+FFFD (pi decodes the file as UTF-8 first)
        got = subprocess.run([DRV], input=b'41 FFFD 42\n', stdout=subprocess.PIPE, check=True).stdout
        self.assertEqual(got, b'41 FFFD 42\n')


if __name__ == '__main__':
    if not os.path.exists(DRV):
        sys.exit('[ERROR] build/unorm_drv missing: make build/unorm_drv')
    unittest.main()
