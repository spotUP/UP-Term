#!/usr/bin/env python3
"""devspeed_rig.py -- DV3 of the console.device plan: a 2000-line `Type` in a
ROM con-handler CON: window (CON OFF), timed with the E-clock
(tests/amiga/stamp), once over the ROM console.device and once over
UP-Term's. Ours must not be slower. Run on the cycle-exact rig
(`rig.py start --exact`: the JIT rig's timing is not an A1200's). Log:
build/rig/shots/devspeed.log."""
import os, pathlib, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"


def run(cmd, timeout=120):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def typeline(s, wait=2.0):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(wait)


def seconds(a, b):
    ha, la, f = (int(x) for x in a.split())
    hb, lb, _ = (int(x) for x in b.split())
    return ((hb << 32 | lb) - (ha << 32 | la)) / f


def timed_type(title):
    run('Delete RAM:t0 RAM:t1 QUIET')
    run('Run >NIL: NewShell "CON:0/20/640/400/%s"' % title)
    time.sleep(6)
    typeline('Execute VTC:dv3.script', 1)
    for _ in range(600):
        time.sleep(1)
        rc, out = run('Type RAM:t1')
        if rc == 0 and out.strip():
            break
    t = seconds(run('Type RAM:t0')[1].strip(), run('Type RAM:t1')[1].strip())
    typeline('endcli', 3)
    return t


def main():
    for f in ("up-console.device", "UPConsole", "stamp"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    (VTC / "lines2000.txt").write_text(''.join(
        'line %04d of 2000: the quick brown fox jumps over the lazy dog\n' % i for i in range(2000)))
    (VTC / "dv3.script").write_text('VTC:stamp RAM:t0\nType VTC:lines2000.txt\nVTC:stamp RAM:t1\n')
    run('VTC:UPConsole CON OFF')
    run('VTC:UPConsole DEVICE OFF')
    rom = timed_type('rom')
    run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
    up = timed_type('upterm')
    run('VTC:UPConsole DEVICE OFF')
    ok = up <= rom
    line = '%s DV3 2000-line Type in a ROM CON: window: ROM device %.1f s, UP-Term device %.1f s' % (
        'ok' if ok else 'FAIL', rom, up)
    print(line)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'devspeed.log').write_text(line + '\n')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
