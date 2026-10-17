#!/usr/bin/env python3
"""FP1.10 P7: the same byte stream into iTerm2 and the host engine, cell by cell.

  iterm_compare.py record   record the scenarios (tools/capture.py's pty + tmux/screen
                            helpers) into build/p7/<name>.80x24.bin
  iterm_compare.py replay   replay each into a NEW iTerm2 window of 80x24 (its own
                            session only; no other window is read, written or
                            resized), write build/p7/<name>.iterm.txt in the vtdump
                            format, close that window
  iterm_compare.py table    tools/refdiff.py's iterm columns for every scenario

Run with build/iterm2-venv/bin/python (module iterm2). Colours are compared as
palette index / default / rgb in the engine's encoding, so themes drop out.
"""
import asyncio, pathlib, shlex, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build/p7"
COLS, ROWS = 80, 24
DEFAULT, RGB = 0x100, 0x1000000
CHAT = "%s %s" % (sys.executable, ROOT / "tools/chatsim.py")
# name -> recorder call; the keys are tmux's / screen's prefix commands
SCENARIOS = ["p7-screen-split", "p7-tmux-split", "p7-chat-log",
             "p7-vim-c-scroll", "p7-man-ls", "p7-colour256"]


def record():
    sys.path.insert(0, str(ROOT / "tools"))
    import capture
    OUT.mkdir(parents=True, exist_ok=True)
    capture.OUT = OUT
    # the host is often loaded (rigs): give every step four times capture.py's settle time
    plain = capture.capture
    capture.capture = lambda *a, settle=0.4, **k: plain(*a, settle=settle * 4, **k)
    capture.screen("p7-screen-split", CHAT + " 30",
                   [b"\x01S", b"\x01\t", b"\x01c", ("%s 40\r" % CHAT).encode(),
                    b"\x01[", b"\x1b[A" * 8, b"\x1b"])
    capture.tmux("p7-tmux-split", CHAT + " 30",
                 [b"\x02\"", ("%s 40\r" % CHAT).encode(), b"\x02%", b"ls -la /\r", b"\x02o",
                  b"\x02[", b"\x1b[A" * 8, b"q"])
    capture.capture("p7-chat-log", [sys.executable, str(ROOT / "tools/chatsim.py"), "120"], [], settle=1.0)
    # FP1.10b: alt screen + scroll region (vim), bold/underline through less, 256 + true colour
    sample = str(ROOT / "engine/vtengine.c")
    capture.capture("p7-vim-c-scroll", ["vim", "-n", "-u", "NONE", "-N", "+syntax on", sample],
                    [b"\x06", b"\x06", b"\x06", b"\x04", b"\x06", b"\x15", b"/scroll_up\r", b"\x06"])
    capture.capture("p7-man-ls", ["man", "ls"], [b" ", b" ", b"b"], env_extra={"MANPAGER": "less", "MANWIDTH": "80"})
    capture.capture("p7-colour256", [sys.executable, str(ROOT / "tools/colorsim.py")], [], settle=1.0)


def colour(c):
    if c is None:
        return DEFAULT
    if c.is_standard:
        return c.standard
    if c.is_rgb:
        r = c.rgb
        return RGB | (r.red << 16) | (r.green << 8) | r.blue
    return DEFAULT


def format_grid(contents):
    text, attr = [], []
    for y in range(ROWS):
        line = contents.line(y)
        chars, cells = [], []
        for x in range(COLS):
            try:
                s = line.string_at(x) or " "
            except IndexError:  # past the last written cell of the line
                s = " "
            st = line.style_at(x)
            chars.append(s)
            if st is None:
                cells.append("%d,%d,0,%d" % (DEFAULT, DEFAULT, ord(s[0])))
                continue
            a = (1 if st.bold else 0) | (8 if st.underline else 0) | (0x20 if st.inverse else 0)
            cells.append("%d,%d,%d,%d" % (colour(st.fg_color), colour(st.bg_color), a, ord(s[0])))
        text.append("".join(chars))
        attr.append(" ".join(cells))
    cur = contents.cursor_coord
    return "\n".join(text + attr + ["@%d,%d" % (cur.x, cur.y), "unhandled 0", ""])


def T(coro):
    """Every iTerm2 API await gets a 20 s limit; a hang raises instead of blocking."""
    return asyncio.wait_for(coro, 20)


def replay_command(stream):
    """Shell command that waits for one line (our resize first), then plays the bytes.

    -opost turns off output processing (ONLCR): the stream must reach iTerm2
    byte for byte. With ONLCR on, every bare LF becomes CR LF; tmux relies on
    bare LF keeping the column, so its pane repaints erased the other pane.
    """
    return "/bin/sh -c %s" % shlex.quote(
        "stty -echo -opost; read x; cat %s; sleep 600" % shlex.quote(str(stream)))


async def replay(connection):
    import iterm2
    for name in SCENARIOS:
        stream = OUT / ("%s.%dx%d.bin" % (name, COLS, ROWS))
        cmd = replay_command(stream)
        window = await T(iterm2.Window.async_create(connection, command=cmd))
        try:
            app = await T(iterm2.async_get_app(connection))
            session = app.get_window_by_id(window.window_id).current_tab.current_session
            await T(session.async_set_grid_size(iterm2.Size(COLS, ROWS)))
            await asyncio.sleep(1.0)
            await T(session.async_send_text("\n"))
            await asyncio.sleep(3.0)
            size = session.grid_size
            assert (size.width, size.height) == (COLS, ROWS), "grid is %s, not 80x24" % (size,)
            contents = await T(session.async_get_screen_contents())
            (OUT / (name + ".iterm.txt")).write_text(format_grid(contents))
            print("[OK] %s" % name)
        finally:
            await T(window.async_close(force=True))


def table():
    sys.path.insert(0, str(ROOT / "tools"))
    import refdiff
    rows = []
    for name in SCENARIOS:
        stream = OUT / ("%s.%dx%d.bin" % (name, COLS, ROWS))
        ours = refdiff.dump("vtdump", stream.read_bytes(), COLS, ROWS)
        ref = refdiff.parse_dump((OUT / (name + ".iterm.txt")).read_text(), ROWS)
        counts, cells = refdiff.iterm_columns(ours, ref, COLS, ROWS)
        print("| %s | %d | %d | %d | %d | %d |" % (name, counts["cells"], counts["text"], counts["attr"],
                                                  counts["colour"], counts["cursor"]))
        for kind in ("text", "attr", "colour"):
            for y, x, k, a, b in [c for c in cells if c[2] == kind][:6]:
                print("    %s row %d col %d vtcon %r iterm %r" % (kind, y, x, a, b))


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "record":
        record()
    elif mode == "replay":
        import iterm2
        iterm2.run_until_complete(replay)
    elif mode == "table":
        table()
    else:
        sys.exit(__doc__)
