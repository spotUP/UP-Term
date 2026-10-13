#!/usr/bin/env python3
"""Aminet readme of one archive: dist/aminet/<id>.readme.in with the values of dist/packages.txt.

  python3 tools/mkreadme.py <id> [--release]   writes build/<archive>.readme and prints its path
  <id> is a package of dist/packages.txt, or "src" (UP-Term-src.lha, versioned with the base)

--release exits 1 while the readme still holds a PLACEHOLDER (an answer the owner owes: the
UP-Term licence line (ledger K4), the uploader address). Without it the readme is written anyway.
Values: @VERSION@ @ARCHIVE@ @BASEREQ@ (the base archive and the lowest version the add-on needs,
empty for the base) @NEEDSBASE@.
"""
import pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pkgs

FIELDS = ('Short', 'Author', 'Uploader', 'Type', 'Version', 'Replaces', 'Architecture', 'Requires', 'Distribution')


def row(pkg_id, table=None):
    """The package's row of packages.txt; "src" is the source archive, versioned with the base."""
    rows = {r['id']: r for r in (pkgs.rows() if table is None else table)}
    if pkg_id == 'src':
        base = rows['base']
        return {'id': 'src', 'archive': 'UP-Term-src', 'version': base['version'], 'needs-base': '-'}
    if pkg_id not in rows:
        raise SystemExit('mkreadme: no package %r in dist/packages.txt' % pkg_id)
    return rows[pkg_id]


def build(pkg_id, root=ROOT, table=None):
    r = row(pkg_id, table)
    values = {'VERSION': r['version'], 'ARCHIVE': r['archive'], 'NEEDSBASE': r['needs-base'],
              'BASEREQ': '' if r['needs-base'] == '-' else 'comm/term/UP-Term.lha >= %s' % r['needs-base']}
    text = (pathlib.Path(root) / 'dist/aminet' / (pkg_id + '.readme.in')).read_text(encoding='latin-1')
    for k, v in values.items():
        text = text.replace('@%s@' % k, v)
    left = sorted({w for w in text.split() if w.startswith('@') and w.endswith('@') and len(w) > 2})
    if left:
        raise SystemExit('mkreadme: %s: no value for %s' % (pkg_id, ' '.join(left)))
    return r, text


def fields(text):
    """{field: value} of the header block (lines `Name: value` up to the first blank line)."""
    out = {}
    for line in text.split('\n\n', 1)[0].splitlines():
        name, _, value = line.partition(':')
        out[name.strip()] = value.strip()
    return out


def main(argv):
    release = '--release' in argv
    ids = [a for a in argv if not a.startswith('--')]
    if len(ids) != 1:
        sys.exit(__doc__)
    r, text = build(ids[0])
    out = ROOT / 'build' / (r['archive'] + '.readme')
    out.parent.mkdir(exist_ok=True)
    out.write_bytes(text.encode('latin-1'))
    print(out)
    if release and 'PLACEHOLDER' in text:
        lines = [l.strip() for l in text.splitlines() if 'PLACEHOLDER' in l]
        sys.exit('mkreadme: %s still has %d placeholder line(s):\n  %s' % (r['archive'], len(lines), '\n  '.join(lines)))


if __name__ == '__main__':
    main(sys.argv[1:])
