#!/usr/bin/env python3
"""claude_datadir_rig.py -- C:Claude's growing files land on disk, not in ENV:
(claude/datadir.c; ENVARC: is copied into RAM at every 3.1 boot).

With the UP-Term: assign present (here: VTC:dd-kit, a host drawer, standing
for the kit's drawer), an older C:Claude's files seeded in ENVARC:Claude
(projects/old-proj/0000002a.jsonl, history, session.json) and one print-mode
run against tools/claude_fixture.py, asserted from the host side of VTC::
  0. without the assign: no "Please insert volume UP-Term:" requester while
     C:Claude runs, everything stays in ENVARC:Claude (the fallback)
  1. the seeded files moved to UP-Term:var/Claude (and are gone from ENVARC:Claude)
  2. the run's own session file is in UP-Term:var/Claude/projects/<slug>/
  3. ENVARC:Claude and ENV:Claude hold no projects/, history or session.json;
     ENVARC:Claude/key (small configuration) is still there
The run is in a visible window titled "Claude data directory check".

  make build/amiga/Claude
  UPTERM_RIG=2 python3 -I tools/rig/rig.py start --max
  UPTERM_RIG=2 python3 -I tools/rig/claude_datadir_rig.py [--port 8091]
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402
import ami  # noqa: E402
import condev_rig as c  # noqa: E402

ROOT = paths.VTCON
VTC = paths.RIG / 'vtc'
# a new drawer each run: the rig's host filesystem caches a drawer deleted on the host side
KIT = VTC / ('dd-kit-%d' % int(time.time()))
TITLE = 'Claude data directory check'
fails = []


def check(name, ok, detail=''):
    print('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, '' if ok else detail), flush=True)
    if not ok:
        fails.append(name)


def run(cmd):
    rc, out = c.run(cmd)
    return rc, out


def kind(path):
    """1 when something is there (on the Amiga), else 0."""
    rc, _ = run('List "%s" QUICK' % path)
    return 0 if rc else 1


def seed():
    """An older C:Claude's ENVARC:Claude."""
    for d in ('ENVARC:Claude', 'ENVARC:Claude/projects', 'ENVARC:Claude/projects/old-proj'):
        run('MakeDir %s' % d)
    run('Echo >ENVARC:Claude/projects/old-proj/0000002a.jsonl "{*"type*":*"user*"}"')
    run('Echo >ENVARC:Claude/history "old prompt"')
    run('Echo >ENVARC:Claude/session.json "{*"messages*":[]}"')
    run('Echo >ENVARC:Claude/key "sk-not-real"')


def requesters():
    """Titles of open windows that are system requesters."""
    out = []
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        if line.startswith('W ') and ('System Request' in line or 'insert volume' in line.lower()):
            out.append(line)
    return out


def one_run(port, label):
    """One print-mode prompt in a visible window; the requesters seen meanwhile."""
    run('Delete RAM:dd-done QUIET')
    script = ('Echo "C:Claude data directory check (%s): one prompt to the fixture"\n'
              'CD RAM:\n'
              'VTC:Claude --url http://127.0.0.1:%d/v1/messages -p hello\n'
              'Echo "rc $RC"\n'
              'Echo >RAM:dd-done "done"\n'
              'Wait 3\n'
              'EndCLI\n' % (label, port))
    with open(VTC / 'dd-check.dos', 'w') as f:
        f.write(script)
    run('Run >NIL: NewShell "CON:0/12/640/200/%s (%s)/CLOSE" FROM VTC:dd-check.dos' % (TITLE, label))
    seen = []
    for _ in range(120):
        seen += [r for r in requesters() if r not in seen]
        if kind('RAM:dd-done') == 1:
            break
        time.sleep(0.5)
    check('the run ended (%s)' % label, kind('RAM:dd-done') == 1)
    time.sleep(4)
    return seen


def count_sessions():
    rc, out = run('List ENVARC:Claude/projects ALL FILES LFORMAT "%N"')
    return sum(1 for l in out.splitlines() if l.strip().endswith('.jsonl'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=8091, help='the fixture\'s port (8080 may be another rig\'s)')
    a = ap.parse_args()
    binary = ROOT / 'build' / 'amiga' / 'Claude'
    if not binary.exists():
        sys.exit('[ERROR] build/amiga/Claude missing: make build/amiga/Claude')
    shutil.copy(binary, VTC / 'Claude')
    KIT.mkdir()
    log = paths.RIG / 'claude_datadir_fixture.log'
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(a.port),
                           '--delay', '0'], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    try:
        # 0. no kit: no UP-Term: assign. C:Claude must not ask for the volume
        # (no "Please insert volume UP-Term:" requester) and keeps everything
        # in ENVARC:Claude, as before
        run('Assign UP-Term: REMOVE')
        seed()
        reqs0 = one_run(a.port, 'no UP-Term: assign')
        check('0 no volume requester without the assign', not reqs0, repr(reqs0))
        check('0 the files stay in ENVARC:Claude', all(kind('ENVARC:Claude/' + n) for n in
                                                      ('projects/old-proj/0000002a.jsonl', 'history', 'session.json')))
        check('0 the run\'s session in ENVARC:Claude/projects', count_sessions() == 2, str(count_sessions()))
        check('0 UP-Term:var was not made', not (KIT / 'var').exists())

        # the kit installed (UP-Term: assigned): the old files move to disk
        run('Delete ENVARC:Claude ENV:Claude ALL QUIET')
        seed()
        run('Assign UP-Term: VTC:%s' % KIT.name)
        reqs1 = one_run(a.port, 'UP-Term: assigned')
        check('no requester with the assign', not reqs1, repr(reqs1))
        var = KIT / 'var' / 'Claude'
        moved = var / 'projects' / 'old-proj' / '0000002a.jsonl'
        check('1 old session moved to UP-Term:var/Claude', moved.is_file(), str(moved))
        check('1 history moved', (var / 'history').is_file() and
              (var / 'history').read_text(errors='replace').startswith('old prompt'))
        check('1 session.json moved', (var / 'session.json').is_file())
        new = [p for p in glob.glob(str(var / 'projects' / '*' / '*.jsonl')) if 'old-proj' not in p]
        check('2 the run\'s session on disk', len(new) == 1, repr(new))
        reqs = [l for l in open(log) if '/v1/messages' in l]
        check('2 one request per run reached the fixture', len(reqs) == 2, repr(reqs))
        for vol in ('ENVARC:Claude', 'ENV:Claude'):
            for name in ('projects', 'history', 'session.json'):
                check('3 %s/%s gone' % (vol, name), kind('%s/%s' % (vol, name)) == 0)
        check('3 ENVARC:Claude/key kept', kind('ENVARC:Claude/key') == 1)
    finally:
        run('Delete UP-Term: ALL QUIET')        # from the Amiga's side, so its cache agrees
        run('Assign UP-Term: REMOVE')
        run('Delete VTC:%s QUIET' % KIT.name)
        run('Delete ENVARC:Claude ENV:Claude RAM:dd-done VTC:dd-check.dos ALL QUIET')
        fx.terminate()
    print('claude_datadir_rig: %s' % ('PASS' if not fails else 'FAIL: ' + ', '.join(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
