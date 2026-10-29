#!/usr/bin/env python3
"""vsh_orphan_job_rig.py -- a background job outlives the vsh that started it (`vsh -c "cmd &"`).

The runner of a job is a process running vsh's own code. The Shell that ran `vsh -c ...` unloaded
that code, and the exit deleted the job's reply port, while the runner had not started yet: the
command never ran and the task "vsh job (...)" waited for ever (rig 9 froze after it; s21, probe 1).
Self-checking: the command must run (RAM:orphan.out has its output) and no "vsh job" task is left.

Second test (memory): vsh's library (vbcc vclib) frees every malloc block at exit (__EXIT_1_malloc ->
LibDeletePool). A job that outlives its shell must not read or free the shell's malloc memory: its
strings are AllocVec. `Wait 4` is the orphan; while it sleeps, other shells run and reuse the freed
memory; when it ends it frees its job. Old code freed into the deleted pool: a Guru or a hang, the
task never left, or a following command failed.

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
    # memory: the orphan sleeps past the shell's exit, others reuse the freed memory, then it ends
    ami('exec', 'GG:bin/sh -c "Wait 4 secs >NIL: &"', '20')
    for i in range(6):
        ami('exec', 'GG:bin/sh -c "Echo churn%d >RAM:orphan2.out ; Type RAM:orphan2.out >NIL:"' % i, '20')
    end = time.time() + 30
    while time.time() < end and job_tasks():
        time.sleep(1)
    left2 = job_tasks()
    ami('exec', 'GG:bin/sh -c "Echo alive >RAM:orphan2.out"', '20')
    got2 = ami('exec', 'Type RAM:orphan2.out', '10')
    ok2 = not left2 and 'alive' in got2
    print('[%s] orphan freed its job after the shell exited: tasks left=%d, system answers=%s'
          % ('OK' if ok2 else 'ERROR', len(left2), 'alive' in got2))
    return 0 if ok and ok2 else 1


sys.exit(main())
