#!/usr/bin/env python3
"""cudump_rig.py -- D3.2 of the console.device plan: the public struct ConUnit
of a unit after the same writes, ROM device against UP-Term's
(tests/amiga/cudump), field by field. Prints both and the lines that differ;
saves build/rig/shots/cudump.log. The rig must be up; `make
build/amiga/cudump build/amiga/up-console.device build/amiga/UPConsole`."""
import os, pathlib, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
OUT = paths.RIG / "shots"


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def main():
    for f in ("cudump", "up-console.device", "UPConsole"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    run('VTC:UPConsole DEVICE OFF')
    rom = run('VTC:cudump')[1]
    run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
    up = run('VTC:cudump')[1]
    run('VTC:UPConsole DEVICE OFF')
    rl, ul = rom.strip().splitlines(), up.strip().splitlines()
    diff = [(a, b) for a, b in zip(rl, ul) if a != b]
    report = ['ROM:'] + rl + ['', 'UP-Term:'] + ul + ['', 'differ: %d of %d' % (len(diff), len(rl))]
    report += ['  ROM %s\n  UP  %s' % d for d in diff]
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'cudump.log').write_text('\n'.join(report) + '\n')
    print('\n'.join(report))
    return 1 if diff or len(rl) != len(ul) else 0


if __name__ == '__main__':
    sys.exit(main())
