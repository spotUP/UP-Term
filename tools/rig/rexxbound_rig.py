#!/usr/bin/env python3
"""rexxbound_rig.py -- rexx_send waits at most REXX_SEND_SECS for a reply (REXX-DEAD-AFTER-THEME).

Entry point: the shared client rexx_send (amarchy/rexx_host.c), driven by RAM:rexxbound against RAM:deafport, an ARexx port
that holds its first message 12 s before replying. Sentinel that the new code ran: rexx_send_timeouts, printed by rexxbound.
Steps: rexxbound's first send returns rc -1 after about 10 s with timeouts=1 (the old code waited for the 12 s reply and
returned rc 0, or forever for a peer that never replies); after the late reply rexx_send_reap frees the parked message once
(twice-called reap is harmless) and a second send answers rc 0. A run that does not finish within 60 s is a FAIL (the hang).
Needs the rig up (rig.py start --max) and build/amiga/{rexxbound,deafport} (make). UPTERM_RIG=<n>."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, '..', '..', 'build', 'amiga')
PROBE = os.environ.get('REXXBOUND_BIN', os.path.join(BUILD, 'rexxbound'))


def main():
    for ln in c.run('Status')[1].splitlines():   # a holder left by an earlier run
        if ln.rstrip().endswith('RAM:deafport'):
            c.run('Break %s C' % ln.split()[1].rstrip(':'))
    time.sleep(1)
    ami.put(PROBE, 'RAM:rexxbound')
    ami.put(os.path.join(BUILD, 'deafport'), 'RAM:deafport')
    c.run('Protect RAM:rexxbound RWED')
    c.run('Protect RAM:deafport RWED')
    c.run('Run >NIL: RAM:deafport DEAF.1 12')
    time.sleep(2)
    t0 = time.time()
    rc, out = c.run('RAM:rexxbound DEAF.1', timeout=60)
    print('INFO rexxbound rc %d after %.0f s:\n%s' % (rc, time.time() - t0, out.strip()))
    c.check(rc == 0 and 'PASS' in out, 'rexx_send gives up after the bound, reaps the late reply, answers again', out[-80:])
    for ln in c.run('Status')[1].splitlines():   # end the port holder: Ctrl-C to its process
        if ln.rstrip().endswith('RAM:deafport'):
            c.run('Break %s C' % ln.split()[1].rstrip(':'))
    time.sleep(1)
    c.run('Delete >NIL: RAM:rexxbound RAM:deafport')
    print('FAILED' if c.failed else 'PASSED')
    return 1 if c.failed else 0


if __name__ == '__main__':
    sys.exit(main())
