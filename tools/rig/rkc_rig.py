#!/usr/bin/env python3
"""rkc_rig.py -- D2.3 of the console.device plan: keys and keymaps through
UP-Term's console.device.

  1. tests/amiga/rkcprobe TABLE: RawKeyConvert over 128 codes x {none,
     Shift, Alt, Ctrl} with the ROM device, and again after UPConsole DEVICE
     ON: the two tables byte-identical (our vector goes to the ROM's).
  2. rkcprobe SWAP: a CHARMAP unit of ours whose keymap has raw keys a and b
     swapped (CD_ASKKEYMAP, CD_SETKEYMAP); typing "a" reads "b", "c" reads
     "c" (the unit converts with its own map, DD12).
  3. DEVICE OFF.
The rig must be up; `make build/amiga/rkcprobe build/amiga/up-console.device
build/amiga/UPConsole` first. Log: build/rig/shots/rkc.log."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"
lines, failed = [], 0


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def check(ok, what, seen=''):
    global failed
    failed += not ok
    lines.append('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + seen.strip()[:400]))
    print(lines[-1], flush=True)
    return ok


def main():
    for f in ("rkcprobe", "up-console.device", "UPConsole"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    run('VTC:UPConsole DEVICE OFF')
    run('VTC:rkcprobe TABLE RAM:rkc-rom.txt')
    rom = run('Type RAM:rkc-rom.txt', 60)[1]
    check(rom.count('\n') == 512, 'the ROM table has 512 lines', str(rom.count('\n')))
    rc, out = run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
    if not check(rc == 0, 'DEVICE ON', out):
        return finish()
    run('VTC:rkcprobe TABLE RAM:rkc-up.txt')
    up = run('Type RAM:rkc-up.txt', 60)[1]
    diff = [(a, b) for a, b in zip(rom.splitlines(), up.splitlines()) if a != b]
    check(rom == up, 'RawKeyConvert through our device equals the ROM\'s (512 lines)',
          '\n'.join('ROM %s | UP %s' % d for d in diff[:10]))
    run('Delete RAM:rkcswap.log QUIET')
    run('Run >NIL: VTC:rkcprobe SWAP')
    for _ in range(20):
        time.sleep(0.5)
        if 'READY' in run('Type RAM:rkcswap.log')[1]:
            break
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'a')
    time.sleep(1.5)
    ami.req(0x08, bytes([4]) + b'c')
    time.sleep(1.5)
    out = run('Type RAM:rkcswap.log')[1]
    reads = ' '.join(re.findall(r'READ ([0-9a-f ]+)', out)).split()
    check(reads[:2] == ['62', '63'], 'a unit with a and b swapped reads "b" for a, "c" for c (%s)' % ' '.join(reads), out)
    time.sleep(12)  # the probe reads for 20 s, then closes its unit
    rc, out = run('VTC:UPConsole DEVICE OFF')
    check(rc == 0, 'DEVICE OFF', out)
    return finish()


def finish():
    lines.append('passed %d of %d' % (sum(1 for l in lines if l.startswith('ok ')),
                                      sum(1 for l in lines if l[:3] in ('ok ', 'FAI'))))
    print(lines[-1])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'rkc.log').write_text('\n'.join(lines) + '\n')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
