#!/usr/bin/env python3
"""conbench_rig.py -- creep's conbench, one console at a time, each after a
fresh boot, the way creep measures CCON (ledger S1, 2026-10-04).

His machine: FS-UAE A1200-Stock (2 MB chip, no fast RAM), AmigaOS 3.2,
Workbench PAL High Res 640x256 16 colours, the console a NewShell window ON
THE WORKBENCH, topaz 8. His line: conbench NAME TO file REPS 3 SCALE 1 SYNC.
So the rig must be up as:
  python3 tools/rig/rig.py start --stock --os32   (and `rig.py aga --stock --os32` once)

  conbench_rig.py [LABEL ...] [--force] [--rows 20|30]

LABELs (default: all of them): ccon127 (CCON 1.2.7, md5 be94e1b5..., his
calibration point), ccon128b9 (his race build), ccon128b1 (github main), upterm (vtcon-handler, Amiga
dialect). 77x20 is a 640x180 window, 77x30 is 640x256.

RESUMABLE: each finished run is written to build/rig/conbench/<label>-<rows>.txt
and skipped next time; --force runs it again. The rows at the end compare
every recorded run.
"""
import pathlib, re, subprocess, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/rig/conbench'
FLAGS = ['--stock', '--os32']

CONSOLES = {
    # label: (mount file or None, window spec with %d for the height)
    'ccon127':   ('VTC:ccon127.mount', 'CC127:0/0/640/%d/CCON127/DEFAULTS'),
    'ccon128b9': ('VTC:ccon128b9.mount', 'CC128:0/0/640/%d/CCON128B9/DEFAULTS'),  # creep's 2026-10-04 build, md5 1fd50e18...
    'ccon128b1': ('VTC:ccon.mount',    'CCON:0/0/640/%d/CCON128/DEFAULTS'),
    'upterm':    (None,                'XCON:0/0/640/%d/UP-Term/AMIGA'),
}
HEIGHT = {20: 180, 30: 256}


def rig(*args):
    subprocess.run([sys.executable, str(ROOT / 'tools/rig/rig.py'), *args, *FLAGS],
                   check=True, timeout=400)


def click(title):
    x, y, w, h = ami.window(title)['box']
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)


def typeline(s):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.5)
    ami.key(0x44)


def run_one(label, rows):
    mount, spec = CONSOLES[label]
    rig('stop')
    rig('start')                      # a fresh boot for every run, as creep does
    if mount:
        dev = spec.split(':')[0]
        c.run('Mount %s: FROM %s' % (dev, mount))
    title = spec.split('/')[4]
    c.run('Delete RAM:cb.txt QUIET')
    c.run('Run >NIL: NewShell "%s"' % (spec % HEIGHT[rows]))
    time.sleep(8)
    click(title)
    typeline('VTC:conbench %s TO RAM:cb.txt REPS 3 SCALE 1 SYNC' % label.upper())
    t0 = time.time()
    while time.time() - t0 < 1800:
        time.sleep(15)
        rc, text = c.run('Type RAM:cb.txt')
        if rc == 0 and 'TOTAL' in text.upper():
            return text
    raise SystemExit('conbench_rig: %s %dx: no TOTAL after 30 min' % (label, rows))


def total(text):
    m = re.search(r'TOTAL\s+\d+\s+([\d.]+)', text, re.I)
    return float(m.group(1)) if m else None


def rows_of(text):
    out = {}
    for line in text.splitlines():
        m = re.match(r'\s*([a-z][a-z0-9-]+)\s+(\d+)\s+([\d.]+)', line)
        if m and m.group(1) != 'total':
            out[m.group(1)] = float(m.group(3))
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    force = '--force' in sys.argv
    nrows = int(sys.argv[sys.argv.index('--rows') + 1]) if '--rows' in sys.argv else 20
    args = [a for a in args if a != str(nrows)]
    labels = args or list(CONSOLES)
    OUT.mkdir(parents=True, exist_ok=True)
    for label in labels:
        f = OUT / ('%s-%d.txt' % (label, nrows))
        if f.exists() and not force:
            print('%-10s %dx: recorded, %s s' % (label, nrows, total(f.read_text())))
            continue
        text = run_one(label, nrows)
        f.write_text(text)
        print('%-10s %dx: %s s' % (label, nrows, total(text)), flush=True)
    done = {p.stem: rows_of(p.read_text()) for p in sorted(OUT.glob('*-%d.txt' % nrows))}
    if done:
        names = list(done)
        tests = list(next(iter(done.values())))
        print('\n%-14s' % 'test' + ''.join('%12s' % n for n in names))
        for t in tests:
            print('%-14s' % t + ''.join('%12s' % done[n].get(t, '-') for n in names))
        print('%-14s' % 'TOTAL' + ''.join('%12s' % total((OUT / (n + '.txt')).read_text()) for n in names))
    return 0


if __name__ == '__main__':
    sys.exit(main())
