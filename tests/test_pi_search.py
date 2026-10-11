#!/usr/bin/env python3
"""amiga-pi's grep and find (claude/search_pi.c, through build/pi_search_drv)
against what pi runs: ripgrep (rg --json --line-number --hidden, the same
ignore rules fd uses) and git (git ls-files --others --exclude-standard)
on random trees with nested .gitignore, .ignore, .rgignore and .fdignore
files, inside and outside a git repository. The rows are compared as sets:
rg's and fd's order is unspecified (a parallel walk). Names are lower case
(amiga-pi matches ignoring case, as AmigaDOS names do)."""
import json, os, random, shutil, subprocess, sys, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DRV = os.path.join(ROOT, 'build', 'pi_search_drv')
GIT = shutil.which('git')
# rg itself, or the one Claude Code carries (run as argv[0] "rg")
RG = shutil.which('rg')
RG_EXE = None
if not RG:
    for c in (os.environ.get('CLAUDE_CODE_EXECPATH', ''), os.path.expanduser('~/.local/bin/claude')):
        if c and os.access(c, os.X_OK):
            RG, RG_EXE = 'rg', c
            break


def run_rg(args, cwd):
    # pi runs rg in its own directory, the agent's working directory
    return subprocess.run([RG] + args, executable=RG_EXE, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

NAMES = ['a', 'b', 'build', 'gen', 'src', 'lib', 'keep', 'x', '.cache', 'docs']
EXTS = ['.txt', '.log', '.c', '.tmp', '']
PATTERNS = ['*.log', '*.tmp', 'build/', '/top.txt', '!keep.log', '**/gen/*', 'a/**/x.c', 'src/*.c', '!src/',
            'lib', '/b', '*.c', '!a.c', 'x*', '.cache/', 'docs/**', '!docs/keep.txt', '#comment', '',
            '\\#hash.txt', 'b/', '!*.txt', '?.txt', '[ab].c', 'gen/**/*.log']


def make_tree(base, rnd, files_ignore):
    paths = []
    for _ in range(rnd.randint(8, 30)):
        depth = rnd.randint(0, 3)
        parts = [rnd.choice(NAMES) for _ in range(depth)]
        name = rnd.choice(NAMES).lstrip('.') + rnd.choice(EXTS)
        if not name:
            continue
        d = os.path.join(base, *parts)
        p = os.path.join(d, name)
        if os.path.isdir(p) or any(os.path.isfile(os.path.join(base, *parts[:i])) for i in range(1, len(parts) + 1)):
            continue
        os.makedirs(d, exist_ok=True)
        with open(p, 'w') as f:
            f.write('one needle here\nnothing\nneedle again\n' if rnd.random() < 0.7 else 'plain\n')
        paths.append(p)
    dirs = [base] + sorted({os.path.dirname(p) for p in paths})
    for d in dirs:
        for kind in files_ignore:
            if rnd.random() < 0.5:
                with open(os.path.join(d, kind), 'w') as f:
                    f.write('\n'.join(rnd.choice(PATTERNS) for _ in range(rnd.randint(1, 5))) + '\n')


def drv(cwd, tool, args):
    p = subprocess.run([DRV, tool, json.dumps(args)], cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out = p.stdout.decode('utf-8', 'replace')
    return p.returncode, [l for l in out.split('\n') if l]


def rg_matches(cwd, extra=()):
    p = run_rg(['--json', '--line-number', '--color=never', '--hidden'] + list(extra) + ['--', 'needle', cwd], cwd)
    rows = set()
    for line in p.stdout.decode().splitlines():
        ev = json.loads(line)
        if ev['type'] == 'match':
            path = os.path.relpath(ev['data']['path']['text'], cwd)
            text = ev['data']['lines']['text'].rstrip('\n')
            rows.add('%s:%d: %s' % (path, ev['data']['line_number'], text))
    return rows


def rg_files(cwd, extra=()):
    p = run_rg(['--files', '--hidden', '--color=never'] + list(extra) + [cwd], cwd)
    return {os.path.relpath(l, cwd) for l in p.stdout.decode().splitlines()}


@unittest.skipUnless(RG and GIT and os.path.exists(DRV), 'needs rg, git and build/pi_search_drv')
class Search(unittest.TestCase):
    def setUp(self):
        self.tmp = os.path.realpath(tempfile.mkdtemp(prefix='pi_search_'))

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def tree(self, seed, repo, kinds, nested=True):
        base = os.path.join(self.tmp, 't%d' % seed)
        os.makedirs(base)
        make_tree(base, random.Random(seed), kinds)
        if repo:
            subprocess.run([GIT, 'init', '-q', base], check=True)
        if nested and seed % 3 == 0:
            # a repository inside: its parents' .gitignore stops at it
            subs = sorted(d for d, _, _ in os.walk(base) if d != base and '/.git' not in d)
            if subs:
                subprocess.run([GIT, 'init', '-q', subs[len(subs) // 2]], check=True)
        return base

    def test_grep_like_rg(self):
        # grep: .gitignore only inside a repository; .ignore and .rgignore always
        bad = []
        for seed in range(60):
            base = self.tree(seed, seed % 2 == 0, ['.gitignore', '.ignore', '.rgignore'])
            rc, rows = drv(base, 'grep', {'pattern': 'needle', 'limit': 100000})
            got = set(r for r in rows if r != 'No matches found')
            want = rg_matches(base)
            if got != want:
                bad.append((seed, sorted(got ^ want)[:4]))
        self.assertEqual(bad, [])

    def test_grep_glob_like_rg(self):
        bad = []
        for seed in range(100, 130):
            base = self.tree(seed, True, ['.gitignore'])
            for g in ['*.c', '!*.log', '**/*.txt', 'src/**']:
                rc, rows = drv(base, 'grep', {'pattern': 'needle', 'glob': g, 'limit': 100000})
                got = set(rows)
                want = rg_matches(base, ['--glob', g])
                if got != want:
                    bad.append((seed, g, sorted(got ^ want)[:4]))
        self.assertEqual(bad, [])

    def test_find_like_fd(self):
        # find: fd's rules (the ignore crate's, as rg --files), --no-require-git outside a repository,
        # .git itself left out; directories carry a '/', rg --files lists files only
        bad = []
        for seed in range(200, 260):
            repo = seed % 2 == 0
            base = self.tree(seed, repo, ['.gitignore', '.ignore', '.fdignore'])
            # rg reads .rgignore, fd .fdignore: the same file under the other name for rg
            for d, _, fs in os.walk(base):
                if '.fdignore' in fs:
                    shutil.copy(os.path.join(d, '.fdignore'), os.path.join(d, '.rgignore'))
            rc, rows = drv(base, 'find', {'pattern': '*', 'limit': 100000})
            got = {r for r in rows if not r.endswith('/') and not r.endswith('.rgignore')}
            want = {f for f in rg_files(base, [] if repo else ['--no-require-git'])
                    if '.git' not in f.split('/') and not f.endswith('.rgignore')}
            if got != want:
                bad.append((seed, sorted(got ^ want)[:4]))
        self.assertEqual(bad, [])

    def test_gitignore_like_git(self):
        # inside a repository with .gitignore files only: what git calls untracked and not ignored
        bad = []
        for seed in range(300, 360):
            base = self.tree(seed, True, ['.gitignore'], nested=False)    # git lists a repository inside as one entry
            rc, rows = drv(base, 'find', {'pattern': '*', 'limit': 100000})
            got = {r for r in rows if not r.endswith('/')}
            p = subprocess.run([GIT, '-C', base, 'ls-files', '--others', '--exclude-standard'],
                               stdout=subprocess.PIPE, check=True)
            want = set(p.stdout.decode().splitlines())
            if got != want:
                bad.append((seed, sorted(got ^ want)[:4]))
        self.assertEqual(bad, [])

    def test_find_path_pattern_and_limit(self):
        base = self.tree(400, False, [])
        os.makedirs(os.path.join(base, 'src', 'deep'))
        for n in ['src/a.spec.ts', 'src/deep/b.spec.ts', 'c.spec.ts']:
            open(os.path.join(base, n), 'w').close()
        rc, rows = drv(base, 'find', {'pattern': 'src/**/*.spec.ts'})
        self.assertEqual(set(rows), {'src/a.spec.ts', 'src/deep/b.spec.ts'})
        rc, rows = drv(base, 'find', {'pattern': '*.spec.ts', 'limit': 2})
        self.assertEqual(len(rows), 2)


if __name__ == '__main__':
    unittest.main()
