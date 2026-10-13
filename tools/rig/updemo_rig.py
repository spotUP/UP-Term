#!/usr/bin/env python3
"""updemo_rig.py -- UPDemo BENCH on the rig, one console at a time (ledger S2).

UPDemo BENCH plays every scene for three seconds and prints the frames a
second each reached (25 is its cap). This runs it in a window of each
console and records the table, so UP-Term and CCON are compared on the
same machine, window and demo.

  updemo_rig.py [LABEL ...] [--tag T] [--force]

LABELs (default all): upterm (XCON:, the xterm dialect), ccon128b9
(VTC:ccon128b9.mount), ccon128b10 (VTC:ccon128b10.mount: creep's
AmigaTools ccon-handler at e82f96a, copied into VTC: by hand).
The rig must be up (rig.py start; the handler under test installed).

RESUMABLE: each run is written to build/rig/updemo/<label>-<tag>.txt
(tag default "now") and skipped next time; --force runs it again. The
recorded runs are compared at the end, scene by scene.
"""
import pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import condev_rig as c

OUT = paths.RIG / 'updemo'
SPEC = '0/0/660/400/%s'
CONSOLES = {
    'upterm':     (None, 'XCON:' + SPEC % 'UPD'),
    'ccon128b9':  ('VTC:ccon128b9.mount', 'CC128:' + SPEC % 'UPD/DEFAULTS'),
    'ccon128b10': ('VTC:ccon128b10.mount', 'CC12A:' + SPEC % 'UPD/DEFAULTS'),
}


def run_one(label):
    mount, spec = CONSOLES[label]
    if mount:
        dev = spec.split(':')[0]
        c.run('Mount %s: FROM %s' % (dev, mount))
    c.run('Delete RAM:upd.txt RAM:upd.done QUIET')
    c.run('Echo "FailAt 21" >RAM:upd.script')
    c.run('Echo "VTC:UPDemo BENCH >RAM:upd.txt" >>RAM:upd.script')
    c.run('Echo "Echo done >RAM:upd.done" >>RAM:upd.script')
    c.run('Echo "EndShell" >>RAM:upd.script')
    c.run('Run >NIL: NewShell "%s" FROM RAM:upd.script' % spec)
    t0 = time.time()
    while time.time() - t0 < 300:
        time.sleep(10)
        rc, _ = c.run('List RAM:upd.done QUICK')
        if rc == 0:
            break
    else:
        raise SystemExit('updemo_rig: %s: no result after 5 min' % label)
    time.sleep(2)
    rc, text = c.run('Type RAM:upd.txt')
    return text


def fps_of(text):
    out = {}
    for line in text.splitlines():
        m = re.match(r'\s+(\S.*?)\s+([\d.]+)\s*$', line)
        if m:
            out[m.group(1)] = float(m.group(2))
    return out


def main():
    args = sys.argv[1:]
    force = '--force' in args
    tag = args[args.index('--tag') + 1] if '--tag' in args else 'now'
    labels = [a for a in args if a in CONSOLES] or list(CONSOLES)
    OUT.mkdir(parents=True, exist_ok=True)
    for label in labels:
        f = OUT / ('%s-%s.txt' % (label, tag))
        if f.exists() and not force:
            print('%-12s recorded' % label)
            continue
        text = run_one(label)
        f.write_text(text)
        print('%-12s %s' % (label, text.splitlines()[0] if text else '(nothing)'), flush=True)
    done = {p.stem: fps_of(p.read_text()) for p in sorted(OUT.glob('*.txt'))}
    if done:
        names = list(done)
        scenes = []
        for d in done.values():
            scenes += [s for s in d if s not in scenes]
        print('\n%-16s' % 'fps' + ''.join('%14s' % n[-14:] for n in names))
        for s in scenes:
            print('%-16s' % s + ''.join('%14s' % done[n].get(s, '-') for n in names))
    return 0


if __name__ == '__main__':
    sys.exit(main())
