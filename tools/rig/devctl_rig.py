#!/usr/bin/env python3
"""devctl_rig.py -- D4.1 and D4.3 of the console.device plan: UPConsole's
DEVICE and EXCLUDE on the rig, refusals included.

  1. DEVICE ON, and ON again: "already".
  2. EXCLUDE CON (the con-handler's task): STATUS lists it; a new CON:
     window's unit is the ROM's (devwho: 0 units of ours); EXCLUDE CLEAR: the
     next window's unit is ours.
  3. DEVICE OFF with a CON: window open: our code stays for that unit
     ("stays"); endcli; the next UPConsole run finds it gone; free memory
     back to the value before DEVICE ON (Avail FLUSH, within 8 KB: the
     UPConsole semaphore and pool slack).
  4. tests/amiga/patchcon SetFunctions RawKeyConvert: DEVICE ON refuses,
     naming vector -48; Ctrl-C restores it; DEVICE ON then works; OFF.
The rig must be up; `make build/amiga/up-console.device build/amiga/UPConsole
build/amiga/devwho build/amiga/patchcon`. Log: build/rig/shots/devctl.log."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
OUT = paths.RIG / "shots"
lines, failed = [], 0
ON = 'VTC:UPConsole DEVICE ON FILE VTC:up-console.device'


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def check(ok, what, seen=''):
    global failed
    failed += not ok
    lines.append('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + seen.strip()[:300]))
    print(lines[-1], flush=True)
    return ok


def typeline(s, wait=2.0):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(wait)


def units():
    m = re.search(r'units (\d+)', run('VTC:devwho')[1])
    return int(m.group(1)) if m else -1


def free():
    run('Avail >NIL: FLUSH')
    run('Avail >NIL: FLUSH')
    m = re.search(r'(\d+)', run('Avail TOTAL')[1])
    return int(m.group(1)) if m else 0


def newshell(title):
    run('Run >NIL: NewShell "CON:0/240/640/150/%s"' % title)
    time.sleep(5)


def main():
    for f in ("up-console.device", "UPConsole", "devwho", "patchcon"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    run('VTC:UPConsole DEVICE OFF')
    run('VTC:UPConsole STATUS')  # the semaphore exists before the baseline
    before = free()
    rc, out = run(ON)
    check(rc == 0 and 'UP-Term now' in out, 'DEVICE ON', out)
    rc, out = run(ON)
    check('already' in out, 'DEVICE ON again says it is on already', out)
    rc, out = run('VTC:UPConsole EXCLUDE CON')
    check(rc == 0, 'EXCLUDE CON', out)
    st = run('VTC:UPConsole STATUS')[1]
    check('excluded (the ROM\'s units): CON' in st, 'STATUS lists CON as excluded', st)
    newshell('excluded')
    check(units() == 0, 'an excluded con-handler window gets the ROM\'s unit (0 of ours)', run('VTC:devwho')[1])
    typeline('endcli', 3)
    run('VTC:UPConsole EXCLUDE CLEAR')
    newshell('ours')
    check(units() == 1, 'after EXCLUDE CLEAR the next window\'s unit is ours', run('VTC:devwho')[1])
    rc, out = run('VTC:UPConsole DEVICE OFF')
    check('stays' in out, 'DEVICE OFF with a unit open: our code stays for it', out)
    st = run('VTC:UPConsole STATUS')[1]
    check('console.device: ROM' in st, 'STATUS after OFF: the ROM\'s', st)
    typeline('echo still here', 2)
    typeline('endcli', 3)
    run('VTC:UPConsole STATUS')  # its run finds the retired device gone
    after = free()
    check(abs(before - after) <= 8192, 'free memory back to before DEVICE ON (%d -> %d)' % (before, after),
          '%d bytes' % (before - after))
    run('Run >NIL: VTC:patchcon')
    time.sleep(2)
    rc, out = run(ON)
    check(rc != 0 and '-48' in out, 'DEVICE ON refuses a patched RawKeyConvert, naming -48', out)
    st = run('Status COM=VTC:patchcon')[1].strip()
    run('Break %s C' % st)
    time.sleep(1)
    rc, out = run(ON)
    check(rc == 0 and 'UP-Term now' in out, 'with the patch gone DEVICE ON works', out)
    rc, out = run('VTC:UPConsole DEVICE OFF')
    check(rc == 0, 'DEVICE OFF', out)
    lines.append('passed %d of %d' % (sum(1 for l in lines if l.startswith('ok ')),
                                      sum(1 for l in lines if l[:3] in ('ok ', 'FAI'))))
    print(lines[-1])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'devctl.log').write_text('\n'.join(lines) + '\n')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
