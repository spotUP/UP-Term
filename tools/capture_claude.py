#!/usr/bin/env python3
"""Record what Claude Code sends to a terminal, with UP-Term's engine answering.

  python3 tools/capture_claude.py [--cols 80] [--rows 24] [--wait 25] [--dir DIR]

Ledger A1.3. Claude Code runs in a pty of a fixed size with TERM=xterm-256color
(what uptelnetd gives it) and no COLORTERM. Every chunk it writes goes through
build/vtreply -- the engine as a live terminal -- and the engine's answers to
its queries (DA1, XTVERSION, the kitty keyboard query, OSC 11) are written
back, so Claude Code picks the features it would pick in an UP-Term window.
The keys: a harmless prompt typed one character at a time ("Reply with the
two words hello amiga..."), Return, a wait for the answer, then Ctrl-C twice.
Every tool is disallowed: the run reads and changes nothing.

The stream is scrubbed before it is saved: the lines Claude Code prints
before its screen (settings warnings, which name the owner's files) are cut,
and the user name and any e-mail address are replaced by as many x's (same
length, so the layout is unchanged). Saved as
tests/streams/claude-session.<cols>x<rows>.bin. Needs `make build/vtreply`.
Each run asks the model once (one short answer); nothing else leaves the Mac.
"""
import argparse, fcntl, os, pathlib, pty, re, select, signal, struct, subprocess, termios, time

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROMPT = b"Reply with the two words hello amiga and nothing else"
TOOLS = "Bash,Edit,Write,MultiEdit,NotebookEdit,WebFetch,WebSearch,Read,Glob,Grep,Task,Agent"


class Engine:
    """build/vtreply: one frame out per chunk, one frame of answers back."""

    def __init__(self, cols, rows):
        self.p = subprocess.Popen([str(ROOT / "build/vtreply"), str(cols), str(rows)],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def feed(self, chunk):
        self.p.stdin.write(struct.pack(">I", len(chunk)) + chunk)
        self.p.stdin.flush()
        n = struct.unpack(">I", self.p.stdout.read(4))[0]
        return self.p.stdout.read(n) if n else b""

    def close(self):
        self.p.stdin.close()
        self.p.wait()


def scrub(data):
    """The stream without the owner's details, every byte offset after the
    cut unchanged in meaning (same-length replacements)."""
    start = data.find(b"\x1b7\x1b[r\x1b8")  # Claude Code's first screen setup
    if start > 0:
        data = data[start:]
    user = os.environ.get("USER", "")
    if len(user) >= 3:
        data = data.replace(user.encode(), b"x" * len(user))
    return re.sub(rb"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}", lambda m: b"x" * len(m.group(0)), data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cols", type=int, default=80)
    ap.add_argument("--rows", type=int, default=24)
    ap.add_argument("--wait", type=float, default=25.0, help="seconds for the answer")
    ap.add_argument("--dir", default=str(ROOT), help="Claude Code's working directory: one it trusts already, or the trust question is all you get")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    eng = Engine(a.cols, a.rows)
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(a.dir)
        env = {k: v for k, v in os.environ.items()
               if k not in ("COLORTERM", "TERM_PROGRAM", "TERM_PROGRAM_VERSION", "ITERM_SESSION_ID",
                            "KITTY_WINDOW_ID", "TMUX", "VTE_VERSION", "WT_SESSION", "LC_TERMINAL",
                            "CLAUDE_CODE_CHILD_SESSION", "CLAUDECODE", "CLAUDE_CODE_ENTRYPOINT")}
        env.update({"TERM": "xterm-256color", "LANG": "en_US.UTF-8", "LC_ALL": "en_US.UTF-8"})
        os.execvpe("claude", ["claude", "--disallowedTools", TOOLS], env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", a.rows, a.cols, 0, 0))
    data = bytearray()
    answers = bytearray()

    def drain(t):
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.05)
            if not r:
                continue
            try:
                c = os.read(fd, 65536)
            except OSError:
                return False
            if not c:
                return False
            data.extend(c)
            ans = eng.feed(c)
            if ans:
                answers.extend(ans)
                os.write(fd, ans)
        return True

    def key(b, t):
        try:
            os.write(fd, b)
        except OSError:
            return False
        return drain(t)

    alive = drain(8)
    for ch in PROMPT:
        if alive:
            alive = key(bytes([ch]), 0.03)
    alive = alive and drain(1) and key(b"\r", a.wait)
    alive = alive and key(b"\x03", 1) and key(b"\x03", 3)
    try:
        os.kill(pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    os.waitpid(pid, 0)
    eng.close()
    out = pathlib.Path(a.out) if a.out else ROOT / "tests/streams" / ("claude-session.%dx%d.bin" % (a.cols, a.rows))
    clean = scrub(bytes(data))
    out.write_bytes(clean)
    print("%s: %d bytes (%d raw); the engine answered %r" % (out.name, len(clean), len(data), bytes(answers)))


if __name__ == "__main__":
    main()
