#!/usr/bin/env python3
"""reboot_deaf_rig.py -- C:Reboot goes ahead when a volume's handler never answers (2026-10-27).

The first Reboot waited for each volume's ACTION_FLUSH with DoPkt. A handler that never answers
(a hung network or FAT handler) held it there for good: no reboot, the Install's last page stuck.
The fixed Reboot sends all the packets at once (SendPkt), waits 3 s at most, names the silent
volumes and reboots.

VTC:deafvol (tests/amiga/deafvol.c) adds a volume DEAF: whose handler port nobody serves.
  A. `Reboot DRYRUN` returns within 15 s, rc 5, and names DEAF with "no answer".
  B. `Reboot` really reboots: RAM:marker is gone once the rig answers again (RAM: is wiped by a
     reset); fail if the marker is still there after 60 s while the rig answers (hung).

Run with the rig up (UPTERM_RIG=9, started --os 32; KS 3.1 on a 060 loops on soft reset):
  python3 tools/rig/reboot_deaf_rig.py
REBOOT_BIN overrides the binary copied as VTC:Reboot (default build/amiga/Reboot).
Verdict log: REBOOT_DEAF_LOG or build/reboot_deaf.log.
"""
import sys, os, time, shutil, pathlib, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
LOG = pathlib.Path(os.environ.get('REBOOT_DEAF_LOG') or ROOT / 'build/reboot_deaf.log')
BIN = pathlib.Path(os.environ.get('REBOOT_BIN') or ROOT / 'build/amiga/Reboot')
passed = total = 0
lines = []


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    s = '%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else '')
    print(s, flush=True)
    lines.append(s)


def try_run(cmd, timeout):
    try:
        return c.run(cmd, timeout)
    except BaseException as e:  # ami.py ends a hung command with SystemExit
        return None, 'exec failed: %r' % (e,)


def main():
    subprocess.run(['nice', '-n', '19', 'make', 'build/amiga/Reboot', 'build/amiga/deafvol'], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    shutil.copyfile(BIN, paths.RIG / 'vtc/Reboot')
    shutil.copyfile(ROOT / 'build/amiga/deafvol', paths.RIG / 'vtc/deafvol')
    c.run('Run >NIL: VTC:deafvol')
    time.sleep(2)
    t0 = time.time()
    rc, out = try_run('VTC:Reboot DRYRUN', 20)
    dt = time.time() - t0
    print(repr(out), rc, '%.1fs' % dt, flush=True)
    check(rc == 5 and dt < 15, 'DRYRUN returns within 15 s with rc 5', '%s rc=%s %.1fs' % (out, rc, dt))
    check('DEAF' in out and 'no answer' in out, 'DRYRUN names DEAF with "no answer"', out)
    # B: the real reboot
    try_run('Echo >RAM:marker x', 10)
    try_run('Run >NIL: VTC:Reboot', 10)
    t0 = time.time()
    verdict = 'no verdict'
    while time.time() - t0 < 120:
        time.sleep(3)
        rc, got = try_run('Type RAM:marker', 5)
        if rc is None:
            continue
        if rc != 0:
            verdict = 'rebooted'
            break
        if time.time() - t0 > 60:
            verdict = 'hung: marker still there after 60 s'
            break
    check(verdict == 'rebooted', 'Reboot reboots with a deaf volume (RAM:marker gone)', verdict)
    print('%d/%d' % (passed, total))
    LOG.write_text('\n'.join(lines) + '\n%d/%d\n' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
