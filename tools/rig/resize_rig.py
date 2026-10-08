#!/usr/bin/env python3
"""resize_rig.py -- tmux and Neovim after an XCON: window resize (R7 follow-up).

Each program runs in an XCON: window under vsh; the rig drags the window's
size gadget and asks the program what size it now believes in; the truth
is what ixwinch (TIOCGWINSZ) reports in the same window afterwards. Passes
when the program's size grew and equals the truth (tmux: window size, one
row less for its status line; nvim: &columns x &lines).
Needs the rig up, the patched ixemul, handler installed, the kit NOT
installed; `make amiga build/amiga/ixwinch`, VTC:tmux from tmux-amiga,
`make dist` in neovim-amiga for nvim (build/m68k/dist).

  resize_rig.py [tmux] [nvim]"""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami, ixpty_rig, paths, ptytest_rig, screen_rig, tmux_rig, winch_rig
from install_rig import run, check
import install_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
NVDIST = paths.repo("neovim-amiga") / "build/m68k/dist"
typeline = screen_rig.typeline
SIZE = re.compile(r'(\d+)x(\d+)')


def size_of(text):
    m = SIZE.search(text)
    return (int(m.group(1)), int(m.group(2))) if m else None


def setup():
    for name in ("pty-handler", "ixwinch"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    shutil.copyfile(tmux_rig.TMUX, VTC / "tmux")
    (VTC / "ptymount").write_text(ptytest_rig.MOUNTLIST)
    (VTC / "tmux.conf").write_text(tmux_rig.CONF)
    (VTC / "gg/bin").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / "build/amiga/vsh", VTC / "gg/bin/sh")
    ixpty_rig.use_ixemul()
    if run('Assign >NIL: GG: EXISTS')[0] != 0:
        run('Assign GG: VTC:gg')
    if run('Assign >NIL: PTY: EXISTS DEVICES')[0] != 0:
        run('Mount PTY: FROM VTC:ptymount')


def open_window(title):
    ami.req(0x02, struct.pack('>H', 10) + ('run >NIL: newshell "XCON:0/20/400/200/%s/CLOSE"' % title).encode())
    time.sleep(4)
    typeline('VTC:vsh', 3)
    return ami.window(title)


def grow(title):
    w = ami.window(title)
    x, y, ww, hh = w['sys:size']
    winch_rig.drag((x + ww // 2, y + hh // 2), 200, 150)
    time.sleep(8)


def truth():
    """TIOCGWINSZ of the window now, from ixwinch, as (cols, rows)."""
    run('Delete RAM:truth QUIET')
    typeline('VTC:ixwinch 1 >RAM:truth', 4)
    return size_of(run('Type RAM:truth')[1])


def tmux_case():
    title = 'rztmux'
    run('Delete RAM:forkprobe.log RAM:tmux_env QUIET')
    if not open_window(title):
        return check(False, 'tmux window opens')
    typeline('VTC:tmux -f /VTC/tmux.conf', 30)
    fmt = "VTC:vsh -c \"VTC:tmux list-windows -F '#{window_width}x#{window_height}'\""
    before = size_of(run(fmt, 30)[1])
    grow(title)
    time.sleep(4)
    after = size_of(run(fmt, 30)[1])
    typeline('VTC:tmux kill-server', 6)
    real = truth()
    ok = bool(before and after and real and after[0] > before[0] and after[1] > before[1]
              and after == (real[0], real[1] - 1))
    print('tmux: before %s after %s truth %s' % (before, after, real))
    check(ok, 'tmux follows the resized window (status line takes a row)',
          'before %s after %s truth %s' % (before, after, real))
    typeline('exit', 2)
    typeline('endcli', 2)
    return ok


def ask_size(ask):
    """nvim's own idea of its size: the command typed from Normal mode, the
    file polled (a try that nvim was not ready for is simply repeated)."""
    for _ in range(4):
        ami.key(0x45)  # Esc: back to Normal mode
        time.sleep(1)
        typeline(ask, 4)
        got = size_of(run('Type RAM:nvsize.txt')[1])
        if got:
            return got
    return None


def nvim_case():
    title = 'rznvim'
    dst = VTC / 'nvim-test'
    if not (NVDIST / 'nvim/bin/nvim').exists():
        return check(False, 'nvim dist exists (make dist in neovim-amiga)')
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(NVDIST, dst)
    (dst / 'rig.vim').write_text(
        "autocmd VimEnter * call writefile(['ready'], 'RAM:nvready.txt')\n"
        "autocmd VimLeave * call writefile(['bye'], 'RAM:nvbye.txt')\n")
    run('Delete RAM:nvready.txt RAM:nvsize.txt RAM:nvbye.txt QUIET')
    if not open_window(title):
        return check(False, 'nvim window opens')
    typeline('export TERM=xterm-256color', 2)
    typeline('VTC:nvim-test/nvim/bin/nvim -u NONE -i NONE -S VTC:nvim-test/rig.vim', 1)
    end = time.time() + 300
    while time.time() < end and 'ready' not in run('Type RAM:nvready.txt')[1]:
        time.sleep(3)
    ask = ':redir! > RAM:nvsize.txt | echo &columns . "x" . &lines | redir END'
    time.sleep(5)
    before = ask_size(ask)
    ami.main(['shot', str(paths.RIG / 'shots/rznvim.png')])
    run('Delete RAM:nvsize.txt QUIET')
    grow(title)
    time.sleep(3)
    after = ask_size(ask)
    typeline(':qa!', 3)
    end = time.time() + 120
    while time.time() < end and 'bye' not in run('Type RAM:nvbye.txt')[1]:
        time.sleep(3)
    time.sleep(3)
    real = truth()
    ok = bool(before and after and real and after[0] > before[0] and after[1] > before[1]
              and after == real)
    print('nvim: before %s after %s truth %s' % (before, after, real))
    check(ok, 'nvim follows the resized window (&columns x &lines)',
          'before %s after %s truth %s' % (before, after, real))
    typeline('exit', 2)
    typeline('endcli', 2)
    return ok


def main():
    which = [a for a in sys.argv[1:] if a in ('tmux', 'nvim')] or ['tmux', 'nvim']
    setup()
    for w in which:
        {'tmux': tmux_case, 'nvim': nvim_case}[w]()
    print('resize_rig: passed %d of %d' % (install_rig.passed, install_rig.total))
    return 0 if install_rig.passed == install_rig.total else 1


if __name__ == '__main__':
    sys.exit(main())
