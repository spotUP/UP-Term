#!/usr/bin/env python3
"""No path component in the kit is longer than 30 characters (the AmigaDOS / classic FFS name limit).

s74/s75: the v1.1 kit held Files/net/bebbossh-1.46.1/patches/0017-shared-secret-normalize.patch (34 characters),
which LhA 1.51 on the Amiga mangled (the 34-character file came out as a directory, 1468 of 1469 files unpacked);
12 other names are 31 or 32 characters. A name over 30 does not fit a classic FFS volume (DOS\\0..DOS\\3, OS 3.1).

The names are counted from the sources that produce them (not from a built archive, so it runs without
`make dist`): every path under dist/ (copied into Files/, including the bebbossh patches and Amarchy themes),
themes/*.conf (copied to Files/themes), and the feed names tools/pkgs.py gives the .ipk files and .lha archives of
dist/packages.txt. A built kit (build/dist/*/), a built feed (build/ipk) and KIT=<dir> are scanned too when present.
"""
import os, pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pkgs

LIMIT = 30


def too_long(base, rel_names):
    return ['%s (%d): %s' % (c, len(c), rel) for rel in rel_names for c in rel.split('/') if len(c) > LIMIT]


def tree(top):
    top = pathlib.Path(top)
    for dirpath, dirs, files in os.walk(top):
        dirs[:] = [d for d in dirs if d != '.git']
        for n in dirs + files:
            yield str((pathlib.Path(dirpath) / n).relative_to(top.parent))


class KitNameLength(unittest.TestCase):
    def test_no_component_over_30_characters(self):
        bad = []
        bad += too_long(ROOT, tree(ROOT / 'dist'))
        bad += too_long(ROOT, ('themes/' + p.name for p in (ROOT / 'themes').glob('*.conf')))
        for r in pkgs.rows():
            bad += too_long(ROOT, ['feed/' + pkgs.ipk_name(r), 'feed/' + r['archive'] + '.lha'])
        extra = [ROOT / 'build/ipk'] + sorted((ROOT / 'build/dist').glob('*')) if (ROOT / 'build/dist').is_dir() else [ROOT / 'build/ipk']
        if os.environ.get('KIT'):
            extra.append(pathlib.Path(os.environ['KIT']))
        for d in extra:
            if d.is_dir():
                bad += too_long(d, tree(d))
        self.assertFalse(bad, '%d path components over %d characters:\n  %s' % (len(set(bad)), LIMIT, '\n  '.join(sorted(set(bad)))))


if __name__ == '__main__':
    unittest.main()
