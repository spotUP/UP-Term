#!/usr/bin/env python3
"""Record what real Unix programs send to an xterm, as test streams.

Each program runs in a pty of a fixed size with TERM=xterm-256color, gets
scripted keystrokes, and everything it writes is saved to
tests/streams/<name>.<cols>x<rows>.bin. Re-run to refresh the captures. The programs are
killed while still on screen (no quit key), so the final grid is theirs:
pyte, the reference, has no alternate screen to return from.
"""
import os, pty, select, struct, fcntl, termios, time, pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "tests/streams"
SAMPLE = ROOT / "engine/vtengine.c"

TERM = "xterm-256color"   # main(): "vtcon" for the terminfo run

def capture(name, argv, keys, cols=80, rows=24, settle=0.4, env_extra=None):
    if TERM == "vtcon":
        name = "ti-" + name
    pid, fd = pty.fork()
    if pid == 0:
        env = {"TERM": TERM, "TERMINFO": str(ROOT / "build/terminfo"), "PATH": os.environ["PATH"], "HOME": os.environ["HOME"],
               "LANG": "en_US.UTF-8", "LC_ALL": "en_US.UTF-8", "LESS": "", "PS1": "$ "}
        if env_extra:
            env.update(env_extra)
        os.execvpe(argv[0], argv, env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
    data = bytearray()
    def drain(t):
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.05)
            if r:
                try:
                    chunk = os.read(fd, 65536)
                except OSError:
                    return False
                if not chunk:
                    return False
                data.extend(chunk)
        return True
    drain(settle * 2)
    for k in keys:
        os.write(fd, k)
        if not drain(settle):
            break
    drain(settle)
    try:
        os.kill(pid, 9)
    except ProcessLookupError:
        pass
    os.waitpid(pid, 0)
    p = OUT / ("%s.%dx%d.bin" % (name, cols, rows))
    p.write_bytes(bytes(data))
    print("%-28s %6d bytes" % (p.name, len(data)))

def tmux(name, cmd, keys, cols=80, rows=24):
    """Run cmd inside tmux (own socket and config under build/), capture the
    outer terminal's bytes: tmux redraws with scroll regions and its own
    status line, the heaviest DECSTBM user there is."""
    sock = str(ROOT / "build/tmux.sock")
    subprocess.run(["tmux", "-S", sock, "kill-server"], capture_output=True)
    capture(name, ["tmux", "-S", sock, "-f", "/dev/null", "new-session", cmd], keys, cols, rows,
            settle=0.6)
    subprocess.run(["tmux", "-S", sock, "kill-server"], capture_output=True)

def screen(name, cmd, keys, cols=80, rows=24):
    sdir = ROOT / "build/screens"
    sdir.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(sdir, 0o700)
    capture(name, ["screen", "-c", "/dev/null", "-S", "vtcon-" + name] + cmd.split(), keys,
            cols, rows, settle=0.6, env_extra={"SCREENDIR": str(sdir)})
    subprocess.run(["screen", "-S", "vtcon-" + name, "-X", "quit"], capture_output=True,
                   env=dict(os.environ, SCREENDIR=str(sdir)))

def vttest_screens():
    """vttest (built by `make vttest` into build/third_party), one capture per
    screen of menus 1 (cursor movements), 2 (screen features) and 8 (VT102
    insert/delete): the grid a test leaves before it asks for Return. Menu
    screens that only print text are included too; they cost nothing."""
    exe = next((ROOT / "build/third_party").glob("vttest-*/vttest"), None)
    if not exe:
        print("vttest not built (make vttest); skipped")
        return
    for old in OUT.glob("vttest-*.bin"):
        old.unlink()
    for menu, screens in ((1, 6), (2, 15), (8, 12)):
        for k in range(screens):
            capture("vttest-m%d-s%02d" % (menu, k), [str(exe)],
                    [("%d\r" % menu).encode()] + [b"\r"] * k, settle=0.3)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    global TERM
    if sys.argv[1:] == ["vttest"]:
        vttest_screens()
        return
    if sys.argv[1:] == ["vtcon"]:
        # The same programs with TERM=vtcon (terminfo/vtcon.terminfo, built
        # into build/terminfo): what they send must all be implemented.
        TERM = "vtcon"
        for old in OUT.glob("ti-*.bin"):
            old.unlink()
    else:
        vttest_screens()
    chat = "%s %s" % (sys.executable, ROOT / "tools/chatsim.py")
    capture("chatsim", [sys.executable, str(ROOT / "tools/chatsim.py"), "60"], [], settle=1.0)
    capture("chatsim-small", [sys.executable, str(ROOT / "tools/chatsim.py"), "30"], [], cols=40,
            rows=12, settle=1.0)
    tmux("tmux-chatsim", chat + " 50", [])
    tmux("tmux-split", chat + " 30",
         [b"\x02\"", ("%s 25\r" % chat).encode(), b"\x02%", b"ls -la /\r", b"\x02o",
          b"\x02[", b"\x1b[A" * 6, b"q"])
    tmux("tmux-vim", "vim -n -u NONE -N " + str(SAMPLE), [b"\x02\"", b"\x06", b"\x02o", b"\x06"])
    screen("screen-chatsim", chat + " 50", [])
    screen("screen-split", chat + " 30",
           [b"\x01S", b"\x01\t", b"\x01c", ("%s 20\r" % chat).encode()])
    f = str(SAMPLE)
    capture("vim-open-scroll", ["vim", "-n", "-u", "NONE", "-N", "+syntax on", f],
            [b"\x06", b"\x06", b"/scroll_up\r", b"\x04", b"\x15", b"G", b"gg"])
    capture("vim-edit", ["vim", "-n", "-u", "NONE", "-N", f],
            [b"10G", b"O", b"inserted line \xc3\xa5\xc3\xa4\xc3\xb6", b"\x1b", b"dd", b"u",
             b"3dd", b":split\r", b"\x17w", b":q!\r"])
    capture("vim-small", ["vim", "-n", "-u", "NONE", "-N", f], [b"\x06", b"\x06", b":q!\r"], cols=40, rows=12)
    capture("less-scroll", ["less", f],
            [b" ", b" ", b"b", b"j" * 5, b"k" * 3, b"/state\r", b"n", b"G"])
    capture("ls-color", ["/bin/ls", "-laG", str(ROOT / "engine"), str(ROOT / "tests")], [],
            env_extra={"CLICOLOR_FORCE": "1"})
    capture("bash-readline", ["/bin/bash", "--norc", "--noprofile", "-i"],
            [b"echo hello world", b"\x1b[D" * 5, b"XX", b"\x01", b"# ", b"\r",
             b"printf '\\e[31mred\\e[0m \\e[1;32mbold green\\e[0m\\n'\r"])
    capture("top", ["/usr/bin/top", "-l", "1", "-n", "5"], [], settle=1.5)

if __name__ == "__main__":
    main()
