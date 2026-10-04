#!/usr/bin/env python3
"""phase_rig.py -- where UP-Term's time goes, per conbench shape (ledger S1).

Needs a handler built with `make build/amiga/vtcon-handler PROF=1 SERIAL=1`
installed in build/rig/vtc, and the stock rig (rig.py start --stock --os32).
Boots fresh, opens the conbench window (XCON:0/0/640/180, Amiga dialect),
runs VTC:phaseprobe in it and prints, per shape, the handler's phases from
the serial log: total, waiting, vt_feed, drawing, the rest -- in ms.

  phase_rig.py [ONLY n]
"""
import pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import condev_rig as c
import conbench_rig as cb

ROOT = pathlib.Path(__file__).resolve().parents[2]
SER = ROOT / 'build/rig/serial.log'


def main():
    only = sys.argv[sys.argv.index('ONLY') + 1] if 'ONLY' in sys.argv else None
    cb.rig('stop')
    cb.rig('start')
    start = SER.stat().st_size if SER.exists() else 0
    c.run('Delete RAM:pp.txt QUIET')
    c.run('Run >NIL: NewShell "XCON:0/0/640/180/UP-Term/AMIGA"')
    time.sleep(8)
    cb.click('UP-Term')
    cb.typeline('VTC:phaseprobe %s>RAM:pp.txt' % (('ONLY %s ' % only) if only else ''))
    t0 = time.time()
    while time.time() - t0 < 900:
        time.sleep(10)
        rc, out = c.run('Type RAM:pp.txt')
        if rc == 0 and out.count('ticks') >= (1 if only else 6):
            break
    time.sleep(2)
    print(out)
    log = SER.read_bytes()[start:].decode('latin-1', 'replace').splitlines()
    name = None
    vals = {}
    for l in log:
        l = l.strip()
        if l in ('plain-lines', 'scroll-nl', 'clear-page', 'sync-line', 'wrap-long', 'bytewise'):
            name = l
            vals = {}
            continue
        m = re.match(r'v PROF (\S+) ([0-9a-f]{8}) ([0-9a-f]{8})', l)
        if m and name:
            vals[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
            if m.group(1) == 'writes/bytes':
                freq, total = vals['eclock/total']
                idle, outt = vals['idle/out']
                rend, frames = vals['render/frames']
                ms = lambda v: 1000.0 * v / freq
                print('%-12s total %7.0f ms  idle %7.0f  feed %7.0f  draw %7.0f (%d frames)  rest %7.0f  writes %d' % (
                    name, ms(total), ms(idle), ms(outt), ms(rend), frames, (total - idle - outt - rend) * 1000.0 / freq, vals['writes/bytes'][0]))
                name = None
    return 0


if __name__ == '__main__':
    sys.exit(main())
