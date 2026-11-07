#!/usr/bin/env python3
"""test_chkabort_suite.py -- every Amiga program that holds system state across a stdio call is in CHKABORT_PROGRAMS.

  python3 tests/test_chkabort_suite.py [--list]

vc.lib's _chkabort ends a program with exit(20) from inside any buffered stdio call when a Ctrl-C is pending,
behind the program's own cleanup (see tests/test_nobreak.py and thoughts/shared/research/s61-evidence/
restart-crash-chkabort.md). A program that leaves a patch, a semaphore, an interrupt server, an input handler, a
notify request, a public port or a broker armed then dies with its state still installed and the next start crashes.

tests/test_nobreak.py proves the listed binaries are free of the library unit. This test proves the LIST is complete:
it reads every $(BUILD)/amiga/<name> target of the Makefile (the dispatch table), collects the C sources of its link
line (variables expanded), and fails when a program whose sources match STATE is neither in CHKABORT_PROGRAMS nor in
EXEMPT below with a written reason. Exit 1 on a miss."""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# System-visible state that outlives the call that made it.
STATE = re.compile(r'\b(SetFunction|AddPort|AddSemaphore|ObtainSemaphore|AddIntServer|IND_ADDHANDLER|StartNotify|'
                   r'CxBroker|AddAppIcon|AddAppMenuItem|AddAppWindow|SetIntVector|ipatch_[a-z_]+|flat_swap_[a-z_]+)\b')

# Programs that match STATE and are not in CHKABORT_PROGRAMS, each with the reason it is safe.
EXEMPT = {
    'UPConsole': 'one-shot tool: its stdio runs before the patch and after the persistent semaphore is added; nothing is pending between',
    'amarchy-index': 'its only state is a public record that is never freed by design; a dead scanner is told from a live one by its task (ix_scan_live)',
    'addrowner': 'SetFunction appears only in a comment; the tool only reads memory under Forbid',
    'taskdump': 'ObtainSemaphore is a read lock held only while it copies task data; no stdio while held',
    'deafport': 'its binary carries no vc.lib _chkabort (test_nobreak.py [OK]); Ctrl-C is taken in its own Wait before RemPort',
    'inhandlers': 'adds and removes its input handler back to back with no stdio between (tests/amiga/inhandlers.c)',
    'inputlist': 'prints only after IND_REMHANDLER; the handler is out before any stdio',
    'palprobe': 'its binary carries no vc.lib _chkabort (test_nobreak.py [OK])',
    'rexxbound': 'client only: sends ARexx commands and opens no host port; a Break leaves nothing armed',
    'AmTile': 'its binary carries no vc.lib _chkabort (test_nobreak.py [OK])',
    'amnotify': 'its binary carries no vc.lib _chkabort (test_nobreak.py [OK])',
    'amarchy-menu': 'gcc/ixemul build: ixemul has its own abort path, the vc.lib unit is not in it (test_nobreak.py [OK])',
    'UPTerm': 'gcc/ixemul or handler build: no vc.lib _chkabort in the binary (test_nobreak.py [OK])',
    'vsh': 'ixemul build: no vc.lib _chkabort in the binary (test_nobreak.py [OK])',
    'vsh_hw': 'ixemul build: no vc.lib _chkabort in the binary (test_nobreak.py [OK])',
    'vtcon-handler': 'DOS handler without stdio: no vc.lib _chkabort in the binary (test_nobreak.py [OK])',
}


def read(path):
    with open(os.path.join(ROOT, path), errors='replace') as f:
        return f.read()


def variables(mk):
    v = {}
    for m in re.finditer(r'^([A-Z0-9_]+)\s*:?=\s*(.*)$', mk, re.M):
        v[m.group(1)] = m.group(2)
    return v


def expand(text, v, depth=0):
    if depth > 8:
        return text
    return re.sub(r'\$\(([A-Z0-9_]+)\)', lambda m: expand(v.get(m.group(1), ''), v, depth + 1), text)


def programs(mk, v):
    """{name: [c sources]} for every $(BUILD)/amiga/<name>: target, from its prerequisites and its recipe."""
    out = {}
    lines = mk.split('\n')
    for i, ln in enumerate(lines):
        m = re.match(r'^\$\(BUILD\)/amiga/([A-Za-z0-9_-]+):(.*)$', ln)
        if not m:
            continue
        text = m.group(2)
        j = i + 1
        while j < len(lines) and lines[j].startswith('\t'):
            text += ' ' + lines[j]
            j += 1
        out[m.group(1)] = sorted(set(re.findall(r'[A-Za-z0-9_./-]+\.c\b', expand(text, v))))
    return out


def chkabort_list(v):
    return set(v['CHKABORT_PROGRAMS'].split())


def audit():
    mk = read('Makefile')
    v = variables(mk)
    listed = chkabort_list(v)
    missing = []
    for name, srcs in sorted(programs(mk, v).items()):
        if name in listed or name in EXEMPT:
            continue
        hits = []
        for s in srcs:
            if not os.path.isfile(os.path.join(ROOT, s)):
                continue
            m = STATE.search(read(s))
            if m:
                hits.append('%s:%s' % (s, m.group(1)))
        if hits:
            missing.append((name, hits[0]))
    return mk, listed, missing


def main():
    mk, listed, missing = audit()
    n = len(programs(mk, variables(mk)))
    for name, why in missing:
        print('[FAIL] %s holds system state (%s) but is not in CHKABORT_PROGRAMS and has no EXEMPT reason' % (name, why))
    print('[INFO] %d Amiga programs, %d in CHKABORT_PROGRAMS, %d exempt' % (n, len(listed), len(EXEMPT)))
    if not missing:
        print('[OK] every program that holds system state is listed or exempt')
    return 1 if missing else 0


if __name__ == '__main__':
    sys.exit(main())
