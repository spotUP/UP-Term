#!/usr/bin/env python3
"""screen_rig.py -- GNU screen (P7.1) on the rig: an XCON window, vsh,
screen with SHELL=vsh and TERM=screen-256color, then tests/amiga/colors.sh
inside it. Passes when all 240 cube and grey cells are the xterm palette
(cube_rig.check) within SCREEN_WAIT seconds (default 20; plain XCON takes
under 4 s, the owner's first run inside screen over 60 s). Screenshot:
build/rig/shots/screen_colors.png.

Then screen's other children, which the port starts with vfork (ixemul has
no fork): a backtick in the status line, the blanker (":blanker") and the
lock (Ctrl-A x with LOCKPRG) and printcmd (through vsh -c; ESC [5i
... ESC [4i in the window) each run tests/amiga/forkprobe, which logs to
RAM:forkprobe.log.

Needs the rig up with the patched ixemul (ixpty_rig.use_ixemul), PTY:
mounted, VTC:screen built from screen-amiga/src in the workspace (Makefile.amiga)
and the kit NOT installed (its DOSDrivers XCON mounts L:vtcon-handler).

  screen_rig.py            run it
  screen_rig.py --keep     leave screen running (default: exit it and close the window)"""
import os, pathlib, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, cube_rig, ixpty_rig, ptytest_rig
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
SHOT = ROOT / "build/rig/shots/screen_colors.png"
SCREENRC = """shell /VTC/vsh
term screen-256color
setenv IXSTACK 65536
startup_message off
backtick 1 0 0 /VTC/forkprobe backtick
hardstatus alwayslastline "status: %1`"
blankerprg /VTC/forkprobe blanker
printcmd "VTC:forkprobe read"
"""
IXPIPE_MOUNT = """IXPIPE:
    Handler = VTC:ixpipe-handler
    Stacksize = 3000
    Priority = 5
    GlobVec = -1
#
"""
CHILDREN = ["argv0=/VTC/forkprobe argc=2 [backtick]",
            "argv0=/VTC/forkprobe argc=2 [blanker]",
            "argv0=SCREEN-LOCK argc=1",
            "argv0=VTC:forkprobe argc=2 [read] stdin=printed"]


def ex(cmd, t=20):
    return ixpty_rig.run(cmd, t)


def typeline(s, wait):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(wait)


def ctrl_a():
    ami.key(0x20, 0x08)  # A with Ctrl
    time.sleep(0.4)


def screens():
    """screen processes on the rig (an attacher and a backend each)"""
    return sum('screen' in l for l in ex('Status')[1].splitlines())


def main():
    shutil.copyfile(ROOT / "tests/amiga/colors.sh", VTC / "colors.sh")
    for name in ("pty-handler", "forkprobe"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    (VTC / "ptymount").write_text(ptytest_rig.MOUNTLIST)
    (VTC / "screenrc").write_text(SCREENRC)
    (VTC / "printcmd.sh").write_text("printf '\\033[5iprinted\\n\\033[4i'\n")
    shutil.copyfile(ROOT / "build/amiga/ixpipe-handler", VTC / "ixpipe-handler")
    (VTC / "ixpipemount").write_text(IXPIPE_MOUNT)
    ixpty_rig.use_ixemul()
    # printcmd's pipe reaches vsh (a native program) through IXPIPE:
    if ex('Assign >NIL: IXPIPE: EXISTS DEVICES')[0] != 0:
        ex('Mount IXPIPE: FROM VTC:ixpipemount')
    if ex('Assign >NIL: PTY: EXISTS DEVICES')[0] != 0:
        ex('Mount PTY: FROM VTC:ptymount')
    ex('Copy VTC:screenrc ENV:screenrc')
    ex('Delete RAM:forkprobe.log QUIET')
    before = screens()
    ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/20/780/560/screen/CLOSE"')
    time.sleep(4)
    typeline('VTC:vsh', 3)
    typeline('LOCKPRG=/VTC/forkprobe; export LOCKPRG', 1)
    typeline('VTC:screen', 10)
    ex('Delete RAM:screen_sty QUIET')
    typeline('echo "$STY $-" >RAM:screen_sty', 2)
    sty = ex('Type RAM:screen_sty')[1].strip()
    if len(sty.split()) < 1 or sty.split()[0] in ('', 'i'):
        print('FAIL screen is not running (no STY in its window)')
        return 1
    rc = 0
    if len(sty.split()) < 2 or 'i' not in sty.split()[1]:
        print('FAIL the window\'s vsh is not interactive ($- without i): ' + sty)
        rc = 1
    typeline('source VTC:colors.sh', 0)
    time.sleep(float(os.environ.get("SCREEN_WAIT", "20")))
    ami.main(['shot', str(SHOT)])
    rc |= cube_rig.check(Image.open(SHOT).convert('RGB'), 30, 580)
    # typed text cannot carry "[" (amiagent types it as "("): from a file
    typeline('source VTC:printcmd.sh', 3)
    ctrl_a()
    typeline(':blanker', 4)
    ami.key(0x40)  # any key ends the blanker
    time.sleep(2)
    ctrl_a()
    ami.req(0x08, bytes([4]) + b'x')
    time.sleep(5)
    log = ex('Type RAM:forkprobe.log')[1]
    for want in CHILDREN:
        ok = want in log
        rc |= not ok
        print('%s screen started %s' % ('ok' if ok else 'FAIL', want))
    if "--keep" not in sys.argv:
        typeline('exit', 3)  # the window's vsh: its last window, so screen ends
        typeline('exit', 2)  # the vsh screen was started from
        typeline('endcli', 2)  # the AmigaDOS Shell of the XCON window
        left = screens()
        if left > before:
            print('FAIL screen still running after exit: %d screen processes, %d before' % (left, before))
            rc = 1
    return rc


if __name__ == '__main__':
    sys.exit(main())
