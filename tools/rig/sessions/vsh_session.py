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
amiga('MakeDir >NIL: ENV:vsh')
amiga('Copy VTC:vshrc ENV:vsh/vshrc')
amiga('Echo >RAM:.vshrc "PS1=\'%F{green}%n@%m%f %F{cyan}%~%f [%?] %# \'"')
amiga('Echo >>RAM:.vshrc "alias ll=\'List\'"')
amiga('SetEnv HOME RAM:')
amiga('SetEnv USER spot')
ami.req(0x02, struct.pack('>H', 10) +
        b'run >NIL: newshell "XCON:0/12/780/560/vsh session/CLOSE"')
time.sleep(4)
line('stack 4096')                     # the 3.1 default: vsh takes its own stack
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
# S3.2: the error stream per command, loaded and through SystemTags (a script)
line("printf '\\033[H\\033[2J'")
line("printf '%s=%03d\\n' x 5")
line('VTC:pkgs/bin/less RAM:nosuch 2>RAM:e; echo "rc $?"; Type RAM:e', 4)
line("echo 'VTC:pkgs/bin/less RAM:nosuch' >RAM:s2; Protect RAM:s2 +s; RAM:s2 2>RAM:e2; Type RAM:e2", 5)
# Ctrl-C reaches a script's Shell process
line("echo 'Wait 30' >RAM:w; Protect RAM:w +s")
t('RAM:w'); ret(2); k(0x33, 0x08); time.sleep(3)
line('echo "script broke $?"')
shot("vsh5f")
# S4.1: a notice before the prompt when a background job ends; fg
line("printf '\\033[H\\033[2J'")
line('Wait 2 &', 1)
line('echo waiting', 3)
line('', 1)                            # the notice comes before this prompt
line('Wait 2 & fg', 4)
line('fg')
shot("vsh5g")
# the Ctrl-C target left behind by a process that ended (the handler kept
# signalling and reading it: a completion worker crashed, #80000008)
t('VTC:breakport'); ret(2); k(0x33, 0x08); time.sleep(4)   # must say "got the break"
line('echo "breakport rc $?"')
t('ech'); time.sleep(2); k(0x41); k(0x41); k(0x41); ret()   # the command check runs, then erased
shot("vsh5h")
# S3.4: subshells are processes of their own
line("printf '\\033[H\\033[2J'")
line('A=1; (A=2; cd SYS:); echo "A=$A"; pwd', 2)
line('echo "sub: $(cd SYS:; pwd)"; pwd', 2)
line('x=$(exit 3); echo "after exit in \\$( ): $?"', 2)
line('{ echo a; echo b; } | while read x; do echo got$x; done', 2)
line('i=0; while [ $i -lt 400 ]; do echo line $i; i=$((i+1)); done | while read a b; do n=$b; done; echo "last $n"', 25)
line('{ Wait 2; echo bg group done; } &', 1)
line('jobs', 4)
line('', 1)
t('while true; do :; done'); ret(2); k(0x33, 0x08); time.sleep(2)
t('while true; do Wait 1; done'); ret(2.5); k(0x33, 0x08); time.sleep(3)
line('echo "loops broke $?"')
shot("vsh5i")
# NAME=v cmd: for that command only; exported: for every command
line("printf '\\033[H\\033[2J'")
line('x=$(exit 3); echo "status $?"')
line('C=5 Get C; Get C; echo "after: $?"', 2)
line('export E=7; Get E', 2)
shot("vsh5j")
# the vshrc's Unix names; deep recursion on the Shell's 4 KB stack
line("printf '\\033[H\\033[2J'")
line('mkdir -p RAM:u/v/w; ls RAM:u/v', 3)
line('cd RAM:u; cd ../u/v; pwd; touch t; ls -l', 3)
line('cd; rm -r RAM:u; ls RAM:u; echo "rm: $?"', 3)
line('f() { if [ $1 -gt 0 ]; then f $(( $1 - 1 )); fi; }; f 40; echo deep ok', 5)
line('g() { g; }; g; echo never', 8)      # stops with an error, the shell goes on
line('echo "still here $?"')
# S9: the console knows vsh's words (functions, builtins, variables)
line("printf '\\033[H\\033[2J'")
line('myfunc() { echo in myfunc; }')
t('myf'); time.sleep(1); k(0x42); time.sleep(2); ret(2)       # Tab: myfunc, runs
t('fg'); time.sleep(2)                                        # green: a builtin
shot("vsh5l")
k(0x16, 0x08); ret(1)                                         # Ctrl-U, empty line
t('echo $HO'); time.sleep(1); k(0x42); time.sleep(2); ret(2)  # Tab: $HOME
t('echo hi | pri'); time.sleep(1); k(0x42); time.sleep(5)     # Tab after |: printf
t("'<%s>\\n'"); ret(2)
t('echo st'); k(0x42); t('X'); time.sleep(5)                   # typed on: the late answer is dropped
shot("vsh5m")
k(0x16, 0x08); ret(1)
shot("vsh5k")
line('exit', 2)
shot("vsh5d")
