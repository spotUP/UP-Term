"""vsh on the rig (vsh plan S5/S6): one scripted session in an XCON window.
Run from tools/rig/sessions with the rig up and build/amiga/vsh installed as
VTC:vsh. Screenshots land in build/rig/shots/vsh*.png."""
import time, ami, struct, os
S = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../build/rig/shots/")
def t(s): ami.req(0x08, bytes([4]) + s.encode('latin-1')); time.sleep(0.4)
def k(code, q=0):
    ami.key(code, q); time.sleep(0.25)
def ret(wait=1.5): k(0x44); time.sleep(wait)
def line(s, wait=1.5): t(s); ret(wait)
def shot(n):
    b = ami.req(0x07); fmt, w, h, nc = b[0], *struct.unpack('>HHH', b[2:8])
    if fmt == 1:
        pal = b[8:8 + nc * 3]; px = b[8 + nc * 3:]
        rows = [b''.join(pal[i * 3:i * 3 + 3] for i in px[y * w:(y + 1) * w]) for y in range(h)]
    else:
        px = b[8:]; rows = [px[y * w * 3:(y + 1) * w * 3] for y in range(h)]
    ami.png(S + n + ".png", w, h, rows); print(S + n + ".png")
def amiga(cmd): return ami.req(0x02, struct.pack('>H', 20) + cmd.encode('latin-1'))[4:].decode('latin-1')

# a user startup file in $HOME (RAM:), and the system one absent
amiga('Delete ENV:vsh/vshrc QUIET')
amiga('Echo >RAM:.vshrc "PS1=\'%F{green}%n@%m%f %F{cyan}%~%f [%?] %# \'"')
amiga('Echo >>RAM:.vshrc "alias ll=\'List\'"')
amiga('SetEnv HOME RAM:')
amiga('SetEnv USER spot')
ami.req(0x02, struct.pack('>H', 10) +
        b'run >NIL: newshell "XCON:0/12/780/560/vsh session/CLOSE"')
time.sleep(4)
line('stack 40000')
line('VTC:vsh', 3)
line('cd T:')                          # outside HOME: the full name
line('cd; pwd')                        # HOME: ~
line('mkdir sub; cd sub; cd ..')
line('echo glob: RAM:.vs*')
line('echo one >f; echo two >>f; Type f')
line('fail() { return 3; }; fail')    # [3] in the prompt
line('ll SYS:S')
shot("vsh5a")
line('Wait 3 &', 1)
line('jobs; wait; echo "bg done $?"', 5)
t('Wait 30'); ret(1.5); k(0x33, 0x08); time.sleep(2)   # Ctrl-C
line('echo "after break $?"')
shot("vsh5b")
line('List SYS:C | VTC:pkgs/bin/less', 4)
shot("vsh5c")
t('q'); time.sleep(2)                  # the reader goes: List must stop too
line('List SYS: ALL | read x; echo "read: $x"', 4)   # a builtin reader, a long writer
shot("vsh5e")
line('exit', 2)
shot("vsh5d")
