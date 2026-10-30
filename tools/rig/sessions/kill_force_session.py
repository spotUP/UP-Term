"""vsh gets its prompt back when a job's task is removed from outside (VSH-KILL-FORCE-HANG, 2026-10-30).
Run (UPTERM_RIG=<n>) with the rig up and the kit installed. A titled XCON window, UP-Term > New tab (vsh),
`Wait 60`, then `wasabi kill --force` (RemTask: no exit code, no reply) on the job's runner, "vsh job".
PASS = within 8 s the tab runs `Echo prompt-is-back` typed after the kill, and the kill took the job
(Wait is gone from ps). Exit status 1 on FAIL."""
import os, subprocess, sys, time
RIG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, RIG)
import ami, typewin
import condev_rig as c

WASABI = os.path.join(RIG, "..", "..", "build", "wasabi", "wasabi")
TITLE = 'kill-force'
bad = 0


def check(ok, what, seen=''):
    global bad
    print(("ok   " if ok else "FAIL ") + what + ('' if ok else ': ' + str(seen).strip()[-300:]), flush=True)
    bad += 0 if ok else 1


def wasabi(*a):
    n = int(os.environ.get('UPTERM_RIG', '1') or 1)
    return subprocess.run([sys.executable, WASABI, '--host', '127.0.0.1', '--port', str(1233 + n), '--key', 'rigwasabi'] + list(a),
                          capture_output=True, text=True).stdout


typewin.open_xcon(TITLE)
wasabi('ui', 'menu', 'UP-Term/New tab')
time.sleep(5)
typewin._last_title = None
typewin.typ('Wait 60')
typewin.key(typewin.RET, wait=3)
ps = wasabi('ps')
jobs = [l for l in ps.splitlines() if 'vsh job' in l]
print('ps vsh job lines: %r' % jobs)
check(len(jobs) == 1, 'one "vsh job" runner is running', ps[-300:])
out = wasabi('kill', '--force', 'vsh job')
print('kill: %r' % out.strip())
time.sleep(4)
typewin.typ('Echo prompt-is-back')
typewin.key(typewin.RET, wait=3)
typewin.typ('UPTerm screen FILE RAM:kf.txt')
typewin.key(typewin.RET, wait=3)
text = c.run('Type RAM:kf.txt')[1]
print('tab text (tail): %r' % text.strip()[-240:])
check('prompt-is-back' in text, 'the prompt came back after the kill and ran Echo', text[-240:])
typewin.typ('exit')
typewin.key(typewin.RET, wait=2)
typewin.typ('EndCLI')
typewin.key(typewin.RET, wait=2)
c.run('Delete >NIL: RAM:kf.txt QUIET')
print('kill_force: %s' % ('FAIL' if bad else 'PASS'))
sys.exit(1 if bad else 0)
