#!/usr/bin/env python3
"""vsh_orphan_job_rig.py -- a background job outlives the vsh that started it (`vsh -c "cmd &"`).

The runner of a job is a process running vsh's own code. The Shell that ran `vsh -c ...` unloaded
that code, and the exit deleted the job's reply port, while the runner had not started yet: the
command never ran and the task "vsh job (...)" waited for ever (rig 9 froze after it; s21, probe 1).
Self-checking: the command must run (RAM:orphan.out has its output) and no "vsh job" task is left.

  UPTERM_RIG=<n> vsh_orphan_job_rig.py"""
import os, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))


def ami(*a):
    return subprocess.run([sys.executable, os.path.join(HERE, 'ami.py')] + list(a),
                          capture_output=True, text=True).stdout


def job_tasks():
    out = subprocess.run([sys.executable, os.path.join(HERE, 'rig.py'), 'wasabi', 'ps', 'vsh job*'],
                         capture_output=True, text=True).stdout
    if 'KIND' not in out:
        sys.exit('[ERROR] rig %s: no task list (%s)' % (os.environ.get('UPTERM_RIG', '?'), out.strip()[:80]))
    return [l for l in out.splitlines() if 'vsh job' in l]


def main():
    ami('exec', 'Delete RAM:orphan.out QUIET', '10')
    ami('exec', 'GG:bin/sh -c "Echo ok >RAM:orphan.out &"', '20')
    end = time.time() + 20
    while time.time() < end and job_tasks():
        time.sleep(1)
    left = job_tasks()
    got = ami('exec', 'Type RAM:orphan.out', '10')
    ok = not left and 'ok' in got
    print('[%s] orphaned job: command ran=%s, tasks left=%d' % ('OK' if ok else 'ERROR', 'ok' in got, len(left)))
    return 0 if ok else 1


sys.exit(main())
