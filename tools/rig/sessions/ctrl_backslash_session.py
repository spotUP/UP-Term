"""Ctrl-\\ in a cooked vtcon window ends the pending read with end of file, as CON: does (D3).
Run (UPTERM_RIG=<n>) with the rig up and build/amiga/vtcon-handler installed.
Cooked AmigaShell in an XCON window: `Copy * TO RAM:cbtest` (not RAM:t: that is the T: directory), type hello Return, then Ctrl-\\ (0x1C).
PASS = the prompt comes back (a marker command runs) and RAM:cbtest holds hello. Before the fix Copy
kept waiting. Also Ctrl-\\ at an empty prompt: the shell ends on end of file, nothing crashes. Exit status 1 on FAIL."""
import os, sys, time, struct
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import ami
def t(s): ami.req(0x08, bytes([4]) + s.encode('latin-1')); time.sleep(0.4)
def k(code, q=0):
    ami.key(code, q); time.sleep(0.25)
def ret(wait=1.5): k(0x44); time.sleep(wait)
def line(s, wait=1.5): t(s); ret(wait)
def amiga(cmd): return ami.req(0x02, struct.pack('>H', 20) + cmd.encode('latin-1'))[4:].decode('latin-1')
BSL, CTRL = 0x0D, 0x08
bad = 0
def check(ok, what):
    global bad
    print(("ok " if ok else "FAIL ") + what)
    bad += 0 if ok else 1

amiga('Delete >NIL: RAM:cbtest RAM:done QUIET')
ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/12/780/560/ctrl-backslash/CLOSE"')
time.sleep(4)
line('Copy * TO RAM:cbtest', 1)
line('hello', 1)
k(BSL, CTRL); time.sleep(2)
line('Echo >RAM:done back', 2)
check('back' in amiga('Type RAM:done'), 'the prompt came back after Ctrl-\\ (Copy ended)')
check('hello' in amiga('Type RAM:cbtest'), 'RAM:cbtest holds hello')
k(BSL, CTRL); time.sleep(2)                 # empty prompt: end of file ends the shell, as on CON:
st = amiga('Status')
check('Shell' not in st and 'Process' in st, 'Ctrl-\\ at an empty prompt: the shell ended (end of file), the system still answers')
amiga('Delete >NIL: RAM:cbtest RAM:done RAM:done2 QUIET')
sys.exit(1 if bad else 0)
