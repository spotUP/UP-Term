#!/usr/bin/env python3
"""closeinactive_rig.py -- the XCON: CLOSEINACTIVE spec field (AM-OSD O2).
A window opened with .../CLOSEINACTIVE closes through the close-gadget path
when it loses focus, and its opener (a Copy reading the window) gets EOF; a
window opened without it stays. Self-checking. The rig must be up with the
current handler installed (rig.py install; UPTERM_RIG=<n>)."""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami


def run(cmd, timeout=30):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def wait_window(title, present, secs=90):
    end = time.time() + secs
    while time.time() < end:
        if bool(ami.window(title)) == present:
            return True
        time.sleep(2)
    return False


def opener(tag, spec):
    """A script that copies the window to RAM and marks RAM:<tag>.done on EOF."""
    run('Delete >NIL: RAM:%s.#?' % tag)
    run('Echo >RAM:%s.s "Copy XCON:%s TO RAM:%s.out*NEcho >RAM:%s.done x"' % (tag, spec, tag, tag))
    run('Run >NIL: Execute RAM:%s.s' % tag)


def done(tag, secs=30):
    end = time.time() + secs
    while time.time() < end:
        if run('List RAM:%s.done' % tag)[0] == 0:
            return True
        time.sleep(2)
    return False


def main():
    bad = 0
    n = str(int(time.time()) % 10000)  # fresh titles every run
    a, b, c = 'ca' + n, 'cb' + n, 'cc' + n
    opener(a, '0/20/300/100/%s/CLOSEINACTIVE' % a)
    ok = wait_window(a, True)
    print('%s %s opened (CLOSEINACTIVE)' % ('ok' if ok else 'FAIL', a))
    if not ok:
        return 1
    time.sleep(3)
    print('active: %s' % bool(ami.window(a) and ami.window(a)['active']))
    opener(b, '320/20/300/100/%s' % b)
    ok = wait_window(b, True)
    print('%s %s opened (takes the focus from %s)' % ('ok' if ok else 'FAIL', b, a))
    gone = wait_window(a, False, 60)
    eof = done(a, 60)
    print('%s %s closed after losing focus: window gone %s, opener EOF %s' % ('ok' if gone and eof else 'FAIL', a, gone, eof))
    bad += not (gone and eof)
    opener(c, '0/140/300/100/%s' % c)
    wait_window(c, True)
    time.sleep(10)
    stays = bool(ami.window(b)) and not done(b, 2)
    print('%s %s (no CLOSEINACTIVE) stays open after losing focus' % ('ok' if stays else 'FAIL', b))
    bad += not stays
    print('FAIL' if bad else 'PASS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
