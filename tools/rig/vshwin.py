"""vshwin.py -- a titled XCON: window that runs VTC:vsh with a throwaway history, for the rig scripts that type
into vsh.

The console keeps a window's line history and appends every entered line to a file: the shell's HISTFILE once
vsh has told it one (ACTION_VTCON_HISTORY VTCON_HIST_FILE), else its default (handler/histfile.h,
UP-Term:var/vtcon/history). Typing `VTC:vsh` at the window's AmigaShell would land in that default, so the window
starts from a script instead: `Set HISTFILE <throwaway>`, `VTC:vsh`, `EndCLI`. Nothing is typed before vsh runs,
vsh hands the console the throwaway file before its first prompt, and the window closes when vsh exits: a script
types `exit` and never `endcli` (that would go to whatever window is active next)."""
import os, re, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import paths

HISTFILE = "RAM:rig.history"   # what the rig's lines are written to (RAM:, gone at the next boot)


def open_vsh(title, box="0/20/600/200", options="", wait=4, before=""):
    """Open XCON:<box>/<title>/CLOSE<options> running VTC:vsh with HISTFILE set; `before` is a line of
    AmigaDOS commands run first (in the script, not typed). Returns after `wait` seconds."""
    name = "vshwin-%s.script" % re.sub(r"[^A-Za-z0-9]", "_", title)
    lines = ["Set HISTFILE %s" % HISTFILE]
    if before:
        lines.append(before)
    lines += ["VTC:vsh", "EndCLI"]
    (paths.RIG / "vtc" / name).write_text("\n".join(lines) + "\n")
    cmd = 'Run >NIL: NewShell "XCON:%s/%s/CLOSE%s" FROM VTC:%s' % (box, title, options, name)
    ami.req(0x02, struct.pack('>H', 10) + cmd.encode('latin-1'))
    time.sleep(wait)


def typeline(s, wait):
    """Type `s` and Return into the active window, then wait `wait` seconds."""
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(wait)


def close_vsh(title, secs=15):
    """End a window open_vsh opened: a click in it makes it active, `exit` ends its vsh and the window
    closes (the script's EndCLI). True once the window is gone, so a run does not leave windows behind."""
    w = ami.window(title)
    if not w:
        return True
    x, y, ww, hh = w['box']
    ami.click_px(x + ww // 2, y + hh // 2)
    time.sleep(1)
    typeline('exit', 0)
    end = time.time() + secs
    while time.time() < end:
        if not ami.window(title):
            return True
        time.sleep(0.5)
    return False
