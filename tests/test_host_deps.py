#!/usr/bin/env python3
"""A host test binary is rebuilt when any header its sources include changes (a header-only change
left build/vsh_host and build/vttest_host stale, so `make test` ran old code).

For each host binary: its sources are read from its link command (`make -n -B`), the project
headers they include are followed (quoted #include, transitively), and for each header
`make -n -W <header> <binary>` must want to rebuild the binary. Run after the binaries are
built (make test builds them first); a binary that is out of date already is reported, not judged."""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGETS = ['build/vttest_host', 'build/vsh_host', 'build/vsh_host_leak', 'build/tn_host', 'build/hl', 'build/mdv']


def make(*args):
    """the commands make -n would run (its "is up to date" notes dropped)"""
    out = subprocess.run(['make', '-n'] + list(args), cwd=ROOT, capture_output=True, text=True).stdout
    return '\n'.join(l for l in out.splitlines() if not re.match(r"make(\[\d+\])?: ", l))


def includes(path, seen):
    try:
        text = open(os.path.join(ROOT, path), encoding='latin-1').read()
    except OSError:
        return
    for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', text, re.M):
        h = os.path.normpath(os.path.join(os.path.dirname(path), inc))
        if not os.path.exists(os.path.join(ROOT, h)):
            h = os.path.normpath(inc)  # -I relative (tests/exec_host)
        if os.path.exists(os.path.join(ROOT, h)) and h not in seen:
            seen.add(h)
            includes(h, seen)


def main():
    fails = 0
    for t in TARGETS:
        if make(t).strip():
            print('[INFO] %s is out of date: build it first' % t)
            continue
        srcs = [w for w in make('-B', t).split() if w.endswith('.c') and os.path.exists(os.path.join(ROOT, w))]
        hdrs = set()
        for s in srcs:
            includes(s, hdrs)
        stale = [h for h in sorted(hdrs) if not make('-W', h, t).strip()]
        for h in stale:
            print('[FAIL] %s does not rebuild when %s changes' % (t, h))
        fails += len(stale)
        if not stale:
            print('[OK] %s: rebuilt for each of its %d headers' % (t, len(hdrs)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
