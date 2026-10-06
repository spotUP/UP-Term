#!/usr/bin/env python3
"""tmux_rig.py -- tmux (P7.2) on the rig, through its top-level entry: an
XCON: window, vsh, tmux with a tmux.conf (vsh as the shell, screen-256color,
a #() status job). Checks:
  - tmux is running and the pane's shell sees $TMUX
  - the 256-colour cube and grey ramp in a pane are the xterm palette
    (cube_rig.check), within TMUX_WAIT seconds
  - a split (Ctrl-B %) makes two panes
  - the #() status job ran (forkprobe logs its start)
  - detach (Ctrl-B d) leaves the session, `tmux attach` brings it back
Needs the rig up with the patched ixemul, PTY: and the kit NOT installed;
VTC:tmux from tmux-amiga in the workspace (make -f Makefile.amiga).

  tmux_rig.py            run it
  tmux_rig.py --keep     leave tmux running"""
import os, pathlib, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, cube_rig, ixpty_rig, paths, ptytest_rig, screen_rig
from PIL import Image
from install_rig import run, check
import install_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
SHOT = ROOT / "build/rig/shots/tmux_colors.png"
TMUX = paths.repo("tmux-amiga") / "build/tmux-bin"
CONF = """set -g default-shell /VTC/vsh
set -g default-terminal screen-256color
set -g status-right "#(VTC:forkprobe status)"
set -g status-interval 5
"""
typeline = screen_rig.typeline


def ctrl_b(key):
    ami.key(0x35, 0x08)  # B with Ctrl
    time.sleep(0.4)
    ami.req(0x08, bytes([4]) + key.encode())
    time.sleep(2)


def lines(cmd):
    return [l for l in run('VTC:vsh -c "%s"' % cmd, 30)[1].splitlines() if l.strip()]


def main():
    shutil.copyfile(TMUX, VTC / "tmux")
    shutil.copyfile(ROOT / "tests/amiga/colors.sh", VTC / "colors.sh")
    for name in ("pty-handler", "forkprobe"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    (VTC / "ptymount").write_text(ptytest_rig.MOUNTLIST)
    (VTC / "tmux.conf").write_text(CONF)
    # the shell ixemul programs run (_PATH_BSHELL in the SDK's paths.h, used
    # by system(), popen() and tmux's #() and run-shell jobs) is /gg/bin/sh,
    # i.e. GG:bin/sh: vsh there, as the kit installs it
    (VTC / "gg/bin").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / "build/amiga/vsh", VTC / "gg/bin/sh")
    ixpty_rig.use_ixemul()
    if run('Assign >NIL: GG: EXISTS')[0] != 0:
        run('Assign GG: VTC:gg')
    if run('Assign >NIL: PTY: EXISTS DEVICES')[0] != 0:
        run('Mount PTY: FROM VTC:ptymount')
    run('Delete RAM:forkprobe.log RAM:tmux_env QUIET')
    ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/20/780/560/tmux/CLOSE"')
    time.sleep(4)
    typeline('VTC:vsh', 3)
    typeline('VTC:tmux -f /VTC/tmux.conf', 30)
    typeline('echo "$TMUX" >RAM:tmux_env', 3)
    env = run('Type RAM:tmux_env')[1].strip()
    check(env.startswith('/'), "tmux runs; the pane's shell has $TMUX", env)
    typeline('source VTC:colors.sh', float(os.environ.get("TMUX_WAIT", "20")))
    ami.main(['shot', str(SHOT)])
    check(cube_rig.check(Image.open(SHOT).convert('RGB'), 30, 560) == 0,
          'the 256-colour cube in a pane is the xterm palette')
    ctrl_b('%')
    panes = lines('VTC:tmux list-panes')
    check(len(panes) == 2, 'Ctrl-B % splits the window into two panes', ' | '.join(panes))
    check('[status]' in run('Type RAM:forkprobe.log')[1], 'the #() status job ran')
    ctrl_b('d')
    time.sleep(2)
    sess = lines('VTC:tmux ls')
    check(len(sess) == 1 and 'attached' not in sess[0], 'detach leaves the session', ' | '.join(sess))
    typeline('VTC:tmux attach', 15)
    sess = lines('VTC:tmux ls')
    check(len(sess) == 1 and 'attached' in sess[0], 'tmux attach brings it back', ' | '.join(sess))
    # kill-server ends the server and, with the panes' ptys closed, their
    # shells (the server ran with every signal blocked: SIGTERM was lost)
    before = sum('tmux' in l or 'vsh' in l for l in run('Status')[1].splitlines())
    rc, out = run('VTC:vsh -c "VTC:tmux -L killtest -f /VTC/tmux.conf new -d"', 120)
    run('VTC:vsh -c "VTC:tmux -L killtest kill-server"', 60)
    time.sleep(8)
    after = sum('tmux' in l or 'vsh' in l for l in run('Status')[1].splitlines())
    check(after <= before, 'kill-server ends the server and its panes',
          '%d tmux/vsh processes, %d before' % (after, before))
    if "--keep" not in sys.argv:
        typeline('exit', 3)      # the second pane
        typeline('exit', 5)      # the first: the session ends, the server too
        typeline('exit', 2)      # the vsh tmux ran from
        typeline('endcli', 2)
    print('tmux_rig: passed %d of %d' % (install_rig.passed, install_rig.total))
    return 0 if install_rig.passed == install_rig.total else 1


if __name__ == '__main__':
    sys.exit(main())
