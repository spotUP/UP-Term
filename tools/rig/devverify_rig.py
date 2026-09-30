#!/usr/bin/env python3
"""devverify_rig.py -- DV2 and DV4 of the console.device plan.

  DV2: tests/amiga/romprobe through the ROM con-handler's CON: (CON OFF),
       once with the ROM device and once with UP-Term's (DEVICE ON): every
       case must put the cursor where the ROM device did.
  DV4: tests/amiga/memprobe: what a unit costs with our device, STANDARD
       and CHARMAP (80 x 25, scrollback full), and whether closing gives it
       all back; the ROM's numbers alongside.
The rig must be up; `make build/amiga/up-console.device build/amiga/UPConsole
build/amiga/romprobe build/amiga/memprobe`. Log: build/rig/shots/devverify.log."""
import os, pathlib, re, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"
lines, failed = [], 0


def run(cmd, timeout=120):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def check(ok, what, seen=''):
    global failed
    failed += not ok
    lines.append('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + seen.strip()[:1200]))
    print(lines[-1], flush=True)


def mem():
    out = {}
    for u in ('STANDARD', 'CHARMAP'):
        m = re.search(r'MEM \w+ open (-?\d+) full (-?\d+) leak (-?\d+)', run('VTC:memprobe ' + u)[1])
        out[u] = tuple(int(x) for x in m.groups()) if m else None
    return out


def main():
    for f in ("up-console.device", "UPConsole", "romprobe", "memprobe"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    shutil.copyfile(ROOT / "tests/probes/amiga_cases.txt", VTC / "amiga_cases.txt")
    run('VTC:UPConsole CON OFF')
    run('VTC:UPConsole DEVICE OFF')
    probe = 'VTC:romprobe VTC:amiga_cases.txt "CON:0/12/656/216/probe"'
    rom = run(probe)[1]
    rom_mem = mem()
    rc, out = run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
    check(rc == 0, 'DEVICE ON', out)
    up = run(probe)[1]
    up_mem = mem()
    run('VTC:UPConsole DEVICE OFF')
    rl = [l for l in rom.splitlines() if l.strip()]
    ul = [l for l in up.splitlines() if l.strip()]
    same = sum(1 for a, b in zip(rl, ul) if a == b)
    check(len(rl) > 1 and len(rl) == len(ul) and same == len(rl),
          'DV2 romprobe through a ROM CON: over our device equals the ROM device on %d of %d lines' % (same, len(rl)),
          '\n'.join('ROM %s | UP %s' % (a, b) for a, b in zip(rl, ul) if a != b))
    for u, limit in (('STANDARD', 64 * 1024), ('CHARMAP', 256 * 1024)):
        m = up_mem[u]
        check(m is not None and m[1] <= limit, 'DV4 %s unit, scrollback full: %s bytes (limit %d; ROM %s)'
              % (u, m[1] if m else '?', limit, rom_mem[u][1] if rom_mem[u] else '?'), str(m))
        check(m is not None and abs(m[2]) <= 4096, 'DV4 %s unit closed: %s bytes not given back (ROM %s)'
              % (u, m[2] if m else '?', rom_mem[u][2] if rom_mem[u] else '?'), str(m))
    lines.append('passed %d of %d' % (sum(1 for l in lines if l.startswith('ok ')),
                                      sum(1 for l in lines if l[:3] in ('ok ', 'FAI'))))
    print(lines[-1])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'devverify.log').write_text('\n'.join(lines) + '\n\nROM mem %s\nUP-Term mem %s\n' % (rom_mem, up_mem))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
