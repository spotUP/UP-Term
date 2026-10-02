#!/usr/bin/env python3
"""soak_rig.py [minutes] [rom] -- DV6 of the console.device plan, with UP-Term's
console.device in (DEVICE ON):

  1. tests/amiga/sigprobe: a task holding signal bit 31 opens and closes a
     unit 100 times and still holds it (the ibmcon 1.8 regression).
  2. The soak: for `minutes` (default 30) a ROM con-handler NewShell window
     after another, each with typed commands (echo, dir, list), then endcli.
     Free memory (Avail FLUSH) after the first round and after the last must
     agree within 16 KB, and no round may fail.
With "rom": the same rounds over the ROM device (the control run: what the
system itself leaks or stalls). Log: build/rig/shots/soak.log (soak-rom.log) (one line per round). The rig must be up;
`make build/amiga/up-console.device build/amiga/UPConsole build/amiga/sigprobe
build/amiga/devwho`."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"


def run(cmd, timeout=120):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def typeline(s, wait=1.5):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(wait)


def free():
    run('Avail >NIL: FLUSH')
    run('Avail >NIL: FLUSH')
    m = re.search(r'(\d+)', run('Avail TOTAL')[1])
    return int(m.group(1)) if m else 0


def units():
    m = re.search(r'units (\d+)', run('VTC:devwho')[1])
    return int(m.group(1)) if m else -1


def main():
    minutes = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
    rom = 'rom' in sys.argv[2:]
    for f in ("up-console.device", "UPConsole", "sigprobe", "devwho"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    OUT.mkdir(parents=True, exist_ok=True)
    log = open(OUT / ('soak-rom.log' if rom else 'soak.log'), 'w')

    def say(s):
        print(s, flush=True)
        log.write(s + '\n')
        log.flush()

    run('VTC:UPConsole CON OFF')
    if rom:
        run('VTC:UPConsole DEVICE OFF')
        ok = True
        say('control run: the ROM device')
    else:
        rc, out = run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
        ok = rc == 0
        say('%s DEVICE ON' % ('ok' if ok else 'FAIL'))
    out = run('VTC:sigprobe', 300)[1]
    sig = 'SIG31 held after 100 opens' in out
    ok &= sig
    say('%s sigprobe: %s' % ('ok' if sig else 'FAIL', ' / '.join(out.strip().splitlines())))
    end = time.time() + minutes * 60
    rounds = bad = 0
    first = None
    stalls = 0
    while time.time() < end:
        rounds += 1
        try:
            run('Run >NIL: NewShell "CON:0/240/640/150/soak%d"' % rounds)
            time.sleep(3)
            typeline('echo soak round %d' % rounds)
            typeline('dir SYS:', 2)
            typeline('list S: QUICK', 2)
            typeline('endcli', 2)
        except (TimeoutError, OSError, SystemExit) as e:
            # a request amiagent did not answer: record what the machine
            # shows, then go on (a stall is a finding, not the end)
            stalls += 1
            say('STALL round %d at %s: %r' % (rounds, time.strftime('%H:%M:%S'), e))
            time.sleep(30)
            try:
                say('  after 30 s: %s' % run('VTC:devwho')[1].strip())
                ami.main(['shot', str(OUT / ('soak-stall-%d.png' % rounds))])
                typeline('endcli', 2)
            except Exception as e2:
                say('  amiagent still silent: %r' % e2)
                break
        n = 0 if rom else units()
        if n != 0:
            bad += 1
            time.sleep(3)
        if rounds == 1:
            first = free()
        say('round %d units after endcli %d' % (rounds, n))
    last = free()
    leak = first - last
    good = bad == 0 and stalls == 0 and abs(leak) <= 16384
    ok &= good
    say('%s %d rounds in %.0f min, %d with a unit left open, %d stalls; free %d after round 1, %d at the end (%+d)'
        % ('ok' if good else 'FAIL', rounds, minutes, bad, stalls, first, last, -leak))
    run('VTC:UPConsole DEVICE OFF')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
