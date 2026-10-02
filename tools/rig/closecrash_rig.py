#!/usr/bin/env python3
"""closecrash_rig.py [rounds] -- the XCON: close-path crash (handoff
2026-10-02): an XCON: window is resized with its zoom gadget (that is what
installs the SIGWINCH input handler close_window takes out again), then
closed with its close gadget; repeated. PASS: amiagent answers after every
round and no window is left. The rig must be up with the current handler
installed (rig.py install)."""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami


def run(cmd, timeout=30):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def wait_window(title, present, secs=150):
    end = time.time() + secs
    while time.time() < end:
        if bool(ami.window(title)) == present:
            return True
        time.sleep(2)
    return False


def click(title, gadget):
    w = ami.window(title)
    x, y, ww, hh = w[gadget]
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((x + ww // 2) * kx), int((y + hh // 2) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    bad = 0
    for i in range(1, rounds + 1):
        t = 'cc%d' % i
        run('Run >NIL: NewShell "XCON:0/20/500/200/%s/CLOSE"' % t)
        if not wait_window(t, True):
            print('FAIL round %d: the window never opened' % i)
            return 1
        time.sleep(2)
        click(t, 'sys:zoom')
        time.sleep(3)
        click(t, 'sys:zoom')
        time.sleep(3)
        click(t, 'sys:close')
        gone = wait_window(t, False, 60)
        try:
            alive = run('Echo alive')[1].strip() == 'alive'
        except Exception:
            alive = False
        ok = gone and alive
        bad += not ok
        print('%s round %d: closed %s, amiagent %s' % ('ok' if ok else 'FAIL', i, gone, 'alive' if alive else 'DEAD'),
              flush=True)
        if not alive:
            break
    print('passed %d of %d' % (rounds - bad, rounds) if alive else 'stopped: the machine stopped answering')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
