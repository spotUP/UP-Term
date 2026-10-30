#!/usr/bin/env python3
"""amarchy_flat_rig.py -- FLAT-C1 rig step (plan 2026-10-29-amarchy-flat-ui-patch.md): the sysiclass h_SubEntry swap of
amarchy-flat running on a 68k (the pass-through trampoline of amarchy/flat_core.c).

Checks, on whatever OS the rig booted (--os 32, --os 31):
  1. amarchy-flat starts, STATUS reads flat=...; sysiclass h_SubEntry (sysiprobe) differs from the original; the
     state block's calls counter rises when windows with sysiclass images open (flatwin N: N open/close cycles).
  2. flatwin's pixel hash of the window frame (close / depth / size gadgets) is the same without and with the swap.
  3. Ctrl-C (Break) removes the swap: h_SubEntry is byte-equal to the value read before the install, STATUS = absent.
  4. No Guru, no hang: every command returns, the rig answers a ping at the end, requesters_in() is empty.
Needs the rig up (UPTERM_RIG=<n> rig.py start --max --os 32|31); make amarchy-flat build/amiga/flatwin
build/amiga/sysiprobe first. Usage: UPTERM_RIG=5 tools/rig/amarchy_flat_rig.py [--cycles N]
Prints one 'ok' / 'FAIL' line per check and 'flat-c1 rig: PASS|FAIL <n> failed'."""
import argparse, os, pathlib, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import condev_rig as c

BUILD = paths.VTCON / 'build/amiga'
failed = 0


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + str(seen).strip()), flush=True)
    return ok


def sub_entry():
    """h_SubEntry of sysiclass as sysiprobe prints it (first HOOK line)."""
    rc, out = c.run('RAM:sysiprobe', 60)
    m = re.search(r'HOOK .*?sub=([0-9a-f]{8})', out)
    return (m.group(1) if m else None), out


def calls():
    out = c.run('RAM:amarchy-flat STATUS')[1]
    m = re.search(r'calls=(\d+)', out)
    return (int(m.group(1)) if m else None), out.strip()


def flatwin(n):
    rc, out = c.run('RAM:flatwin %d' % n, 600)
    m = re.search(r'hash=([0-9a-f]{8})', out)
    return (m.group(1) if m else None), out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cycles', type=int, default=1000)
    a = ap.parse_args()
    for n in ('amarchy-flat', 'flatwin', 'sysiprobe'):
        ami.put(str(BUILD / n), 'RAM:' + n)
    c.run('Stack 262144')
    iv = c.run('Version FULL')[1].strip().replace('\n', ' ')
    print('rig:', iv, flush=True)
    sub0, out = sub_entry()
    check(sub0 is not None, 'sysiprobe reads the original h_SubEntry (%s)' % sub0, out)
    hash_off, out = flatwin(1)
    check(hash_off is not None, 'frame hash without the patch %s' % hash_off, out)
    c.run('Run >NIL: RAM:amarchy-flat')
    time.sleep(3)
    n0, st = calls()
    check(n0 is not None and 'flat=' in st, 'amarchy-flat STATUS: ' + st, st)
    sub1, out = sub_entry()
    check(sub1 is not None and sub1 != sub0, 'h_SubEntry swapped %s -> %s' % (sub0, sub1), out)
    hash_on, out = flatwin(1)
    n1, _ = calls()
    check(n1 is not None and n1 > n0, 'calls rose with one window: %s -> %s' % (n0, n1))
    check(hash_on == hash_off, 'frame pixels identical with the patch (%s vs %s)' % (hash_off, hash_on))
    h2, out = flatwin(a.cycles)
    n2, _ = calls()
    check(n2 is not None and n2 - n1 >= a.cycles, '%d open/close cycles: calls %s -> %s' % (a.cycles, n1, n2))
    check(h2 == hash_off, 'hash after %d cycles unchanged (%s)' % (a.cycles, h2))
    st = c.run('Status')[1]
    m = re.search(r'Process\s+(\d+):.*amarchy-flat', st)
    check(bool(m), 'amarchy-flat is a CLI process', st)
    if m:
        c.run('Break %s C' % m.group(1))
        time.sleep(3)
    sub2, out = sub_entry()
    check(sub2 == sub0, 'Ctrl-C restores h_SubEntry byte-equal (%s vs %s)' % (sub0, sub2), out)
    check('absent' in calls()[1], 'STATUS after removal: ' + calls()[1], calls()[1])
    hash_after, out = flatwin(1)
    check(hash_after == hash_off, 'frame hash after removal unchanged (%s)' % hash_after)
    check(not ami.requesters(), 'no requester / Guru on screen', ami.requesters())
    check(c.run('Version')[0] == 0, 'rig still answers (no hang)')
    print('flat-c1 rig: %s %d failed' % ('PASS' if not failed else 'FAIL', failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
