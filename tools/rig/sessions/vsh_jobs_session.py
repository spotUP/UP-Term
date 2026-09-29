"""vsh_jobs_session.py -- vsh S8 on the rig: ^Z suspends an ixemul job,
fg and bg continue it, jobs lists it; a native command is refused; less
(a full-screen program) comes back in its own mode after fg. Needs the
patched ixemul (put first in LIBS: for this boot), VTC:vsh, VTC:ixkill in
the command path (C: here), VTC:ixwait. Shots vshj1..vshj3 in
build/rig/shots. Run with the rig up after `make amiga`."""
import os, pathlib, shutil, struct, sys, time
RIG = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(RIG))
import ami, ixpty_rig

ROOT = RIG.parents[1]
SHOTS = ROOT / "build/rig/shots"
def t(s): ami.req(0x08, bytes([4]) + s.encode('latin-1')); time.sleep(0.4)
def ret(wait=1.5): ami.key(0x44); time.sleep(wait)
def line(s, wait=1.5): t(s); ret(wait)
def ctrl(code, wait=1.5): ami.key(code, 0x08); time.sleep(wait)
def shot(n): ami.main(['shot', str(SHOTS / (n + '.png'))])
def amiga(cmd): return ami.req(0x02, struct.pack('>H', 20) + cmd.encode('latin-1'))[4:].decode('latin-1')
Z, C = 0x31, 0x33

for name in ("vsh", "ixkill"):
    shutil.copyfile(ROOT / "build/amiga" / name, ixpty_rig.VTC / name)
ixpty_rig.use_ixemul()
amiga('Copy VTC:ixkill C:ixkill')
amiga('MakeDir >NIL: ENV:vsh')
amiga('Copy VTC:vshrc ENV:vsh/vshrc')
ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/12/780/560/vsh jobs/CLOSE"')
time.sleep(4)
line('stack 100000')
line('VTC:vsh', 3)
line('VTC:ixwait', 2.5); ctrl(Z)                 # [1] Stopped  VTC:ixwait (one prompt)
line('jobs')
line('fg', 3); ctrl(Z)                           # counts on, stopped again
line('bg', 3)                                    # [1] VTC:ixwait &, dots behind the prompt
line('jobs')
shot('vshj1')
time.sleep(18); ret()                            # [1] Done (the machine rebooted here before)
shot('vshj1b')
line('VTC:ixbg keys', 2); t('a'); time.sleep(1)  # [61]
ctrl(Z)                                          # stopped inside a raw read
line('echo prompt reads its own line')           # not taken by the stopped job's read
line('fg', 2); t('b'); time.sleep(1)             # [62], raw again, no echo
t('q'); time.sleep(1.5)
line('Wait 20', 1); ctrl(Z)                      # native: refused
ctrl(C)                                          # ^C ends it
line('echo done')
shot('vshj2')
