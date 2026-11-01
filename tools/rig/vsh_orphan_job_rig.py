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

Third test (subshell): `( ... ) &` is a "vsh subshell" process running the interpreter on vsh's own
malloc heap (shell/vsh_mem.c) with a malloc'd shell and parse tree. It must outlive the shell too: the
code stays loaded, and the heap is deleted by the last orphan, not by the exiting shell.

Fourth test (code leak): the code kept for the orphans goes when the last of them ends (orphan_end has
DOS unload it). Old code never unloaded it: every shell that orphaned a job lost about 240 KB of free
memory for good. Five orphaning shells (jobs and subshells) may now cost at most 64 KB in all.

Fifth test (kill, VSH-JOB-SIGNALS): `sh -c "VTC:ixsigexit sleep &"` leaves an ixemul sleeper running in an
orphaned job; another shell sends it `kill -TERM $!` (the pid the first shell wrote). The job must end
within 10 s (the sleeper's own sleep is 30 s) and the system must still answer. Before the orphan fix the
runner of such a job waited for ever on its dead parent's port: the kill left "vsh job" behind and the
rig froze (s21). Needs build/amiga/ixsigexit (copied to VTC:ixsigexit here). After the orphan fix the
job still hung in 1 of 2 tries: its error stream was the shell's own handle, which the caller (wasabid's
exec file) Closes when the shell returns, and the killed sleeper writes "Terminated" through it. Other
shells allocate in between (churn), which reuses that freed handle; KILL_ROUNDS rounds (default 4).

Sixth test (order, VSH-JOB-SIGNALS follow-up): a background job has its own handle on the shell's file
output (separate offset), so the shell moves its handle on to the job's end when it reaps it.
`echo one; echo two & wait; echo three` into a file (the AmigaDOS `>T:file` of the exec) must read one,
two, three; also with `wait $!` and with an external command (C:Type) as the job. Fails as "one three"
when the reap does not move the shell's handle on.

  UPTERM_RIG=<n> vsh_orphan_job_rig.py      ONLY=kill | ONLY=order: one test
  VSH=<host path>: the vsh under test (default build/amiga/vsh), copied to VTC:vshtest; the orphaning
  and the order shells run it (a gate refreshes GG:bin/sh from the build, so an older vsh goes there)"""
import os, shutil, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import paths

VSH = os.environ.get('VSH') or os.path.join(HERE, '..', '..', 'build', 'amiga', 'vsh')
SH = 'VTC:vshtest'


def put_vsh():
    shutil.copyfile(VSH, str(paths.RIG / 'vtc' / 'vshtest'))


def ami(*a):
    return subprocess.run([sys.executable, os.path.join(HERE, 'ami.py')] + list(a),
                          capture_output=True, text=True).stdout


def job_tasks():
    out = subprocess.run([sys.executable, os.path.join(HERE, 'rig.py'), 'wasabi', 'ps', 'vsh job*'],
                         capture_output=True, text=True).stdout
    if 'KIND' not in out:
        sys.exit('[ERROR] rig %s: no task list (%s)' % (os.environ.get('UPTERM_RIG', '?'), out.strip()[:80]))
    return [l for l in out.splitlines() if 'vsh job' in l]


def sub_tasks():
    out = subprocess.run([sys.executable, os.path.join(HERE, 'rig.py'), 'wasabi', 'ps', 'vsh subshell*'],
                         capture_output=True, text=True).stdout
    if 'KIND' not in out:
        sys.exit('[ERROR] rig %s: no task list (%s)' % (os.environ.get('UPTERM_RIG', '?'), out.strip()[:80]))
    return [l for l in out.splitlines() if 'vsh subshell' in l]


def free_total():
    out = ami('exec', 'Avail TOTAL', '10')
    nums = [l.strip() for l in out.splitlines() if l.strip().isdigit()]
    if not nums:
        sys.exit('[ERROR] rig %s: Avail TOTAL gave no number (%s)' % (os.environ.get('UPTERM_RIG', '?'), out.strip()[:80]))
    return int(nums[-1])


def orphans_gone():
    end = time.time() + 30
    while time.time() < end and (job_tasks() or sub_tasks()):
        time.sleep(1)
    return not job_tasks() and not sub_tasks()


def code_leak():
    # one round first, so that the libraries and commands the round loads are in memory before the count
    ami('exec', 'GG:bin/sh -c "Wait 1 secs >NIL: &"', '20')
    orphans_gone()
    before = free_total()
    for i in range(3):
        ami('exec', 'GG:bin/sh -c "Wait 1 secs >NIL: &"', '20')
    for i in range(2):
        ami('exec', 'GG:bin/sh -c "( Wait 1 secs >NIL: ) &"', '20')
    gone = orphans_gone()
    lost = before - free_total()
    ok = gone and lost <= 64 * 1024
    print('[%s] code of orphaning shells unloaded: 5 shells lost %d bytes (limit 65536), orphans ended=%s'
          % ('OK' if ok else 'ERROR', lost, gone))
    return ok


def kill_orphan():
    shutil.copyfile(os.path.join(HERE, '..', '..', 'build', 'amiga', 'ixsigexit'), str(paths.RIG / 'vtc' / 'ixsigexit'))
    for r in range(int(os.environ.get('KILL_ROUNDS', '4'))):
        if not kill_round(r):
            return False
    return True


def kill_round(r):
    ami('exec', 'Delete RAM:kpid QUIET', '10')
    ami('exec', SH + ' -c "VTC:ixsigexit sleep & echo $! >RAM:kpid"', '20')
    # churn: other shells allocate and free, reusing whatever the exited shell's caller freed
    for i in range(4):
        ami('exec', 'GG:bin/sh -c "Echo churn%d >RAM:orphan5.out ; Type RAM:orphan5.out >NIL:"' % i, '20')
    time.sleep(1)
    running = len(job_tasks())
    pid = ami('exec', 'Type RAM:kpid', '10').strip().splitlines()
    pid = pid[-1].strip() if pid else ''
    if not pid.isdigit():
        print('[ERROR] killed orphan: no pid in RAM:kpid (%r)' % pid)
        return False
    ami('exec', 'GG:bin/sh -c "kill -TERM %s"' % pid, '20')
    end = time.time() + 10
    while time.time() < end and job_tasks():
        time.sleep(1)
    left = job_tasks()
    ami('exec', 'GG:bin/sh -c "Echo alive5 >RAM:orphan5.out"', '20')
    alive = 'alive5' in ami('exec', 'Type RAM:orphan5.out', '10')
    ok = running == 1 and not left and alive
    print('[%s] killed orphaned job, round %d: running before kill=%d, tasks left 10 s after kill -TERM=%d, system answers=%s'
          % ('OK' if ok else 'ERROR', r + 1, running, len(left), alive))
    return ok


def order():
    ok = True
    ami('exec', 'Echo two >T:vshtwo.txt', '10')
    for line in ('echo one; echo two & wait; echo three',
                 'echo one; echo two & wait $!; echo three',
                 'echo one; C:Type T:vshtwo.txt & wait; echo three'):
        ami('exec', 'Delete T:vshorder.out QUIET', '10')
        ami('exec', '%s >T:vshorder.out -c "%s"' % (SH, line), '20')
        got = [l.strip() for l in ami('exec', 'Type T:vshorder.out', '10').splitlines() if l.strip() and not l.startswith('rc ')]
        good = got == ['one', 'two', 'three']
        ok = ok and good
        print('[%s] background job output kept after the reap: `%s` >T:file gave %s'
              % ('OK' if good else 'ERROR', line, ' '.join(got)))
    ami('exec', 'Delete T:vshorder.out T:vshtwo.txt QUIET', '10')
    return ok


def main():
    put_vsh()
    if os.environ.get('ONLY') == 'kill':
        return 0 if kill_orphan() else 1
    if os.environ.get('ONLY') == 'order':
        return 0 if order() else 1
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
    # subshell: the orphan sleeps past the shell's exit, others reuse the memory, then it runs on
    ami('exec', 'Delete RAM:sub.out QUIET', '10')
    ami('exec', 'GG:bin/sh -c "( Wait 3 secs >NIL: ; Echo sub >RAM:sub.out ) &"', '20')
    for i in range(4):
        ami('exec', 'GG:bin/sh -c "Echo churn%d >RAM:orphan3.out ; Type RAM:orphan3.out >NIL:"' % i, '20')
    end = time.time() + 30
    while time.time() < end and (sub_tasks() or 'sub' not in ami('exec', 'Type RAM:sub.out', '10')):
        time.sleep(1)
    left3 = sub_tasks()
    got3 = ami('exec', 'Type RAM:sub.out', '10')
    ami('exec', 'GG:bin/sh -c "Echo alive3 >RAM:orphan3.out"', '20')
    alive3 = 'alive3' in ami('exec', 'Type RAM:orphan3.out', '10')
    ok3 = not left3 and 'sub' in got3 and alive3
    print('[%s] orphaned subshell: command ran=%s, tasks left=%d, system answers=%s'
          % ('OK' if ok3 else 'ERROR', 'sub' in got3, len(left3), alive3))
    ok4 = code_leak()
    ok6 = order()
    ok5 = kill_orphan()
    return 0 if ok and ok2 and ok3 and ok4 and ok5 and ok6 else 1


sys.exit(main())
