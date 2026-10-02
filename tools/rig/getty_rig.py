#!/usr/bin/env python3
"""getty_rig.py -- ledger T4: a Unix shell over the serial port. This script
is the terminal on the other end of the cable: it holds a host pty, boots
the rig with FS-UAE's serial port on that pty (rig.py --serial), starts
C:upgetty on the Amiga (vsh on a PTY: pair behind serial.device) and then
talks to vsh over the wire:

  1. vsh's prompt arrives over serial.
  2. `echo getty-$((6*7))` comes back as getty-42 (the shell runs, output
     returns).
  3. `echo $TERM` is xterm-256color (upgetty's TERM reached the shell).
  4. `stty size`-like: `echo $LINES $COLUMNS` is not required; the window
     size is checked by ixemul's view instead: VTC:ixtty's TIOCGWINSZ
     prints 24 x 80 (the size upgetty set on the master).
  5. Ctrl-C over the wire stops `Wait 30` (ISIG: the line discipline's
     break), the prompt is back within 5 s.
  6. exit: the shell ends, upgetty ends (Status no longer lists it).
The rig is stopped before and after (its serial port changes). Log:
build/rig/shots/getty.log."""
import os, pathlib, re, select, shutil, struct, subprocess, sys, time, tty
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"
RIG = [sys.executable, str(ROOT / "tools/rig/rig.py")]
lines, failed = [], 0


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def check(ok, what, seen=''):
    global failed
    failed += not ok
    lines.append('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + repr(seen)[-400:]))
    print(lines[-1], flush=True)
    return ok


class Wire:
    """The host end of the serial cable."""

    def __init__(self):
        self.master, slave = os.openpty()
        # raw: a cooked host pty turns the 0x03 we send into its own SIGINT
        # (and echoes "^C") -- it never reached the Amiga (2026-10-02)
        tty.setraw(slave)
        self.path = os.ttyname(slave)
        self.slave = slave  # kept open: FS-UAE opens it by path
        self.seen = b''

    def send(self, s):
        os.write(self.master, s.encode('latin-1') if isinstance(s, str) else s)

    def wait_for(self, pattern, seconds):
        end = time.time() + seconds
        while time.time() < end:
            r, _, _ = select.select([self.master], [], [], 0.5)
            if r:
                try:
                    self.seen += os.read(self.master, 4096)
                except OSError:
                    pass
                if re.search(pattern, self.seen):
                    return True
        return False

    def text(self):
        return self.seen.decode('latin-1', 'replace')


def main():
    for f in ("upgetty", "ixtty"):
        src = ROOT / "build/amiga" / f
        if src.exists():
            shutil.copyfile(src, VTC / f)
    wire = Wire()
    subprocess.run(RIG + ["stop"], capture_output=True)
    subprocess.run(RIG + ["start", "--serial", wire.path], check=True)
    import ixpty_rig
    ixpty_rig.use_ixemul()
    if run('Assign >NIL: PTY: EXISTS DEVICES')[0] != 0:
        run('Mount PTY: FROM VTC:ptymount')
    run('Run >NIL: VTC:upgetty SHELL VTC:vsh')
    prompt = rb'[%#$>] $'
    check(wire.wait_for(prompt, 30), 'vsh\'s prompt arrives over serial', wire.text())
    wire.seen = b''
    wire.send('echo getty-$((6*7))\r')
    check(wire.wait_for(rb'getty-42', 15), 'a command runs and its output comes back', wire.text())
    wire.wait_for(prompt, 5)
    wire.seen = b''
    wire.send('echo $TERM\r')
    check(wire.wait_for(rb'xterm-256color', 10), 'TERM is xterm-256color in the shell', wire.text())
    wire.wait_for(prompt, 5)
    wire.seen = b''
    wire.send('VTC:ixtty\r')
    ok = wire.wait_for(rb'TIOCGWINSZ 0: 80 x 24', 60)
    check(ok, 'ixemul sees the size upgetty set (80 x 24)', wire.text())
    wire.send('x')  # ixtty's "press a key"
    wire.wait_for(prompt, 15)
    wire.seen = b''
    wire.send('Wait 30\r')
    for _ in range(30):  # the command must be running: vsh drops a Ctrl-C that comes before it starts
        time.sleep(1)
        if 'Wait' in run('Status')[1]:
            break
    time.sleep(1)
    t0 = time.time()
    wire.send(b'\x03')
    back = wire.wait_for(prompt, 8)
    check(back and time.time() - t0 < 6, 'Ctrl-C over the wire breaks Wait 30 (%.1f s)' % (time.time() - t0),
          wire.text())
    wire.seen = b''
    wire.send('exit\r')
    time.sleep(4)
    st = run('Status')[1]
    check('upgetty' not in st, 'exit ends the shell and upgetty', st)
    lines.append('passed %d of %d' % (sum(1 for l in lines if l.startswith('ok ')),
                                      sum(1 for l in lines if l[:3] in ('ok ', 'FAI'))))
    print(lines[-1])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'getty.log').write_text('\n'.join(lines) + '\n')
    subprocess.run(RIG + ["stop"], capture_output=True)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
