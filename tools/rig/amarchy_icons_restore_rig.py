#!/usr/bin/env python3
"""amarchy-icons APPLY then RESTORE on a rig: every .info of the walked volumes byte-identical afterwards (s28 row
"RESTORE byte-for-byte check").
  UPTERM_RIG=<n> tools/rig/rig.py start --max --os 32 --purpose "<session>: icon RESTORE byte check"
  UPTERM_RIG=<n> python3 tools/rig/rig.py install     (the script puts md5sum in RAM: and lucide.dat in UP-Term:Amarchy/Icons/ when the rig has none)
  UPTERM_RIG=<n> python3 tools/rig/amarchy_icons_restore_rig.py [--vols SYS:,VTCX:,BOOTX:,Work:] [--watch SYS:Utilities.info]
Steps: List every .info per volume (ALL), md5sum them on the Amiga, APPLY, md5 again (how many changed), RESTORE, md5
again, compare with the first list. Prints counters, rc, and for each differing file size and md5 before and after."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ami, condev_rig as c

MD5 = 'RAM:md5sum'
CHUNK = 10


def infos(vol):
    rc, out = c.run('List %s PAT=#?.info ALL FILES LFORMAT="%%p%%n" QUICK' % vol, 120)
    if rc > 5:
        print('[WARN] List %s rc %d' % (vol, rc))
    return sorted(l.strip() for l in out.splitlines() if l.strip().lower().endswith('.info'))


def sums(files):
    got = {}
    for i in range(0, len(files), CHUNK):
        part = files[i:i + CHUNK]
        rc, out = c.run(MD5 + ' ' + ' '.join('"%s"' % f for f in part), 120)
        for l in out.splitlines():
            m = re.match(r'^([0-9a-f]{32})\s+\*?(.+?)\s*$', l.strip())
            if m:
                got[m.group(2)] = m.group(1)
    return got


def snapshot(files):
    s = sums(files)
    miss = [f for f in files if f not in s]
    if miss:
        print('[WARN] no md5 for %d files, first: %s' % (len(miss), miss[:3]))
    return s


def main():
    vols = ['SYS:', 'VTCX:', 'BOOTX:', 'Work:']
    if '--vols' in sys.argv:
        vols = sys.argv[sys.argv.index('--vols') + 1].split(',')
    files = []
    for v in vols:
        files += infos(v)
    files = [f for f in files if '/Storage/Icons/Amarchy' not in f and 'Icons/Amarchy' not in f]
    print('[INFO] %d .info files on %s' % (len(files), vols))
    dat = ami.read_file('UP-Term:Amarchy/Icons/lucide.dat')
    print('[INFO] UP-Term:Amarchy/Icons/lucide.dat on the rig: %s' % ('missing' if dat is None else '%d bytes' % len(dat)))
    if dat is None:   # a rig without the kit: the dat from dist/ (make dist), as the kit ships it
        c.run('MakeDir VTCX:Apps/Amarchy')
        c.run('MakeDir VTCX:Apps/Amarchy/Icons')
        ami.put(os.path.join(HERE, '..', '..', 'dist', 'Amarchy', 'Icons', 'lucide.dat'), 'UP-Term:Amarchy/Icons/lucide.dat')
    if '--outline' in sys.argv and ami.read_file('UP-Term:Amarchy/Icons/outline.dat') is None:
        ami.put(os.path.join(HERE, '..', '..', 'dist', 'Amarchy', 'Icons', 'outline.dat'), 'UP-Term:Amarchy/Icons/outline.dat')
    ami.put(os.path.join(HERE, '..', '..', 'build', 'dist', 'up-term', 'Files', 'userland', 'bin', 'md5sum'), 'RAM:md5sum')
    c.run('Protect RAM:md5sum +e')
    ami.put(os.path.join(HERE, '..', '..', 'build', 'amiga', 'amarchy-icons'), 'RAM:amarchy-icons')
    c.run('Protect RAM:amarchy-icons +e')
    before = snapshot(files)
    rc, out = c.run('RAM:amarchy-icons APPLY', 300)
    print('[INFO] APPLY rc %d: %s' % (rc, out.strip()))
    mid = snapshot(files)
    print('[INFO] changed by APPLY: %d' % sum(1 for f in before if before[f] != mid.get(f)))
    if '--outline' in sys.argv:   # the Icon style pref set to Outline, then the same APPLY the desk trigger runs on a style change
        c.run('MakeDir ENV:Amarchy')
        pf = os.path.join(os.environ.get('TMPDIR', '/tmp'), 'icon-style-prefs')
        open(pf, 'w').write('[profile default]\nicon-style = outline\n')
        ami.put(pf, 'ENV:Amarchy/prefs')
        rc, out = c.run('RAM:amarchy-icons APPLY', 300)
        print('[INFO] APPLY (Outline) rc %d: %s' % (rc, out.strip()))
        sty = snapshot(files)
        print('[INFO] Outline differs from Filled: %d, from the originals: %d, of %d' % (
            sum(1 for f in before if sty.get(f) != mid.get(f)), sum(1 for f in before if sty.get(f) != before[f]), len(before)))
    rc, out = c.run('RAM:amarchy-icons RESTORE', 300)
    print('[INFO] RESTORE rc %d: %s' % (rc, out.strip()))
    after = snapshot(files)
    for w in sys.argv[sys.argv.index('--watch') + 1].split(',') if '--watch' in sys.argv else []:
        print('[WATCH] %s before %s apply %s after %s' % (w, before.get(w), mid.get(w), after.get(w)))
    bad = [f for f in before if before[f] != after.get(f)]
    for f in bad:
        sz = lambda p: len(ami.read_file(p) or b'')
        print('[FAIL] %s before %s after %s apply %s size-now %d' % (f, before[f], after.get(f), mid.get(f), sz(f)))
    print('[%s] byte-identical %d of %d, differing %d' % ('PASS' if not bad else 'FAIL', len(before) - len(bad),
                                                      len(before), len(bad)))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
