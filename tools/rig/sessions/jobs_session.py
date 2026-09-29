"""jobs_session.py -- job control against the terminal under tcsh on the
patched ixemul (P6): a background read stops (SIGTTIN), a background write
with TOSTOP stops (SIGTTOU), fg resumes both, ^Z then bg keeps a job
running. Screenshots jobs1..jobs3 in build/rig/shots are the result.
Run with the rig up; `make amiga build/amiga/ixbg` first."""
import os, pathlib, shutil, struct, sys, time
RIG = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(RIG))
import ami, ixpty_rig

SHOTS = RIG.parents[1] / "build/rig/shots"

def t(s): ami.req(0x08, bytes([4]) + s.encode('latin-1')); time.sleep(0.4)
def ret(): ami.key(0x44); time.sleep(1.5)
def shot(n): ami.main(['shot', str(SHOTS / (n + '.png'))])

for name in ("ixbg",):
    shutil.copyfile(RIG.parents[1] / "build/amiga" / name, ixpty_rig.VTC / name)
ixpty_rig.use_ixemul("--orig" in sys.argv)
ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/12/640/228/jobs/CLOSE"'); time.sleep(4)
t('stack 100000'); ret()
t('VTC:pkgs/shells/tcsh'); ret(); time.sleep(5)
# tcsh's "&" needs fork() (ixemul: "No more processes"): jobs go to the
# background with ^Z and bg, while ixbg waits its two seconds
def run_to_bg(cmd):
    """cmd, ^Z inside its three-second wait, bg; it then touches the terminal"""
    t(cmd); ami.key(0x44); time.sleep(0.8)
    ami.key(0x31, 0x08); time.sleep(1.5)
    t('bg'); ret(); time.sleep(3); ret()
run_to_bg('VTC:ixbg read')                                 # SIGTTIN: Suspended (tty input)
t('fg'); ret(); t('typed in fg'); ret(); time.sleep(1)     # read: typed in fg
run_to_bg('VTC:ixbg tostop')                               # SIGTTOU: Suspended (tty output)
t('fg'); ret(); time.sleep(1)                              # tostop: printed
shot("jobs1")
t('VTC:ixwait'); ret(); time.sleep(3)
ami.key(0x31, 0x08); time.sleep(1.5)                       # ^Z
t('bg'); ret(); time.sleep(3)                              # dots go on behind the prompt
t('jobs'); ret(); time.sleep(1)
shot("jobs2")
