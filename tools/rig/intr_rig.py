#!/usr/bin/env python3
"""intr_rig.py -- the characters typed after a Ctrl-C that ended a read
(todos: "lost first character after Ctrl-C during an interrupted read").

Runs tests/amiga/ixintr in an XCON: window, presses Ctrl-C while its read
waits, types "echo hi2" and Return, and reads VTC:intr.out. Passes when
the bytes the program read after the Ctrl-C are exactly "echo hi2\\n".
Modes: line (default termios) and raw (one byte per read).
The rig must be up, handler installed; `make amiga build/amiga/ixintr`.

  intr_rig.py [line|raw|vsh ...]"""
import os, pathlib, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami, ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
OUT = VTC / "intr.out"
TEXT = "echo hi2"


def read_out():
    try:
        return OUT.read_text()
    except OSError:
        return ''


def got_bytes(out):
    """The bytes the program read once SIGINT had arrived, from the log."""
    data = b''
    for l in out.splitlines():
        f = l.split()
        if len(f) >= 5 and f[0] == 'read' and int(f[1]) > 0 and int(f[4]) > 0:
            data += bytes(int(h, 16) for h in f[5:])
    return data


def case(mode):
    title = 'intr' + mode
    (VTC / "intr.script").write_text('VTC:ixintr 20 %s\nEndCLI\n' % ('raw' if mode == 'raw' else 'line'))
    if OUT.exists():
        OUT.unlink()
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/400/150/%s/CLOSE" FROM VTC:intr.script' % title, 10)
    end = time.time() + 30
    while time.time() < end and 'ready' not in read_out():
        time.sleep(1)
    w = ami.window(title)
    if 'ready' not in read_out() or not w:
        print('%s: FAIL (program or window did not start)' % title)
        return False
    time.sleep(2)
    ami.key(0x33, 0x08)  # C with Ctrl
    time.sleep(3)
    ami.req(0x08, bytes([4]) + TEXT.encode())
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(4)
    out = read_out()
    got = got_bytes(out)
    want = (TEXT + '\n').encode()
    print('%s: log:\n  %s' % (title, out.strip().replace('\n', '\n  ')))
    print('%s: read after Ctrl-C %r: %s' % (title, got, 'ok' if got == want else 'FAIL (want %r)' % want))
    ami.key(0x44)  # make sure the program ends
    return got == want


def vsh_case():
    """Ctrl-C ends a child that waits in read(); the shell's next line must
    arrive whole ("cho" for "echo" was the symptom)."""
    import screen_rig
    ixpty_rig.run('Delete RAM:intr_echo QUIET')
    ixpty_rig.run('Run >NIL: NewShell "XCON:0/20/600/200/intrvsh/CLOSE"', 10)
    time.sleep(4)
    screen_rig.typeline('VTC:vsh', 3)
    screen_rig.typeline('VTC:ixintr 20 die', 3)
    ami.key(0x33, 0x08)
    time.sleep(3)
    screen_rig.typeline('echo hi2 >RAM:intr_echo', 3)
    ami.main(['shot', str(paths.RIG / 'shots/intrvsh.png')])
    rc, out = ixpty_rig.run('Type RAM:intr_echo')
    ok = out.strip() == 'hi2'
    print('intrvsh: line after Ctrl-C ran as typed: %s (file %r)' % ('ok' if ok else 'FAIL', out.strip()))
    screen_rig.typeline('exit', 1)
    screen_rig.typeline('endcli', 1)
    return ok


def main():
    if 'vsh' in sys.argv[1:]:
        shutil.copyfile(ROOT / "build/amiga/ixintr", VTC / "ixintr")
        ixpty_rig.use_ixemul()
        return 0 if vsh_case() else 1
    modes = [a for a in sys.argv[1:] if a in ('line', 'raw')] or ['line', 'raw']
    shutil.copyfile(ROOT / "build/amiga/ixintr", VTC / "ixintr")
    ixpty_rig.use_ixemul()
    ok = True
    for m in modes:
        ok = case(m) and ok
    print('intr_rig: %s' % ('OK' if ok else 'FAIL'))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
