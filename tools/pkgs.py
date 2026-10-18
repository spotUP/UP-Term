#!/usr/bin/env python3
"""The package table dist/packages.txt, for the Makefile, mkinstaller, mkmanifest and the tests.

  python3 tools/pkgs.py names              ids, one a line
  python3 tools/pkgs.py archives           archive names, one a line
  python3 tools/pkgs.py get <id> <column>  one value ("-" is printed as it is)
  python3 tools/pkgs.py index <dir>        upkg's index of the archives in <dir>
"""
import hashlib, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TABLE = ROOT / 'dist/packages.txt'
COLUMNS = ('id', 'archive', 'version', 'needs-base', 'ixemul', 'record', 'handed', 'consumes')


def parse(text):
    """[{column: value}] from the table text; a row must have every column."""
    out = []
    for line in text.splitlines():
        line = line.rstrip()
        if not line or line.startswith('#'):
            continue
        f = re.split(r'\s{2,}', line.strip())
        if len(f) != len(COLUMNS):
            raise SystemExit('packages.txt: %r: want %s' % (line, ' '.join(COLUMNS)))
        out.append(dict(zip(COLUMNS, f)))
    return out


def rows(path=TABLE):
    return parse(pathlib.Path(path).read_text(encoding='utf-8'))


def get(pkg_id, column, path=TABLE):
    if column not in COLUMNS:
        raise SystemExit('pkgs: no column %r (have %s)' % (column, ' '.join(COLUMNS)))
    for r in rows(path):
        if r['id'] == pkg_id:
            return r[column]
    raise SystemExit('pkgs: no package %r' % pkg_id)


def index(build, path=TABLE):
    """upkg's index (upkg plan D1): a line per archive, fields one space apart:
    id archive version needs-base record size sha256 of <build>/<archive>.lha.
    Released next to the archives; an archive missing from <build> is refused."""
    out = ['# upkg index 1: id archive version needs-base record size sha256']
    for r in rows(path):
        lha = pathlib.Path(build) / (r['archive'] + '.lha')
        if not lha.is_file():
            raise SystemExit('pkgs index: %s missing: make dist' % lha)
        data = lha.read_bytes()
        out.append(' '.join((r['id'], r['archive'], r['version'], r['needs-base'], r['record'],
                             str(len(data)), hashlib.sha256(data).hexdigest())))
    return '\n'.join(out) + '\n'


def main(argv):
    if argv[:1] == ['names'] and len(argv) == 1:
        print('\n'.join(r['id'] for r in rows()))
    elif argv[:1] == ['archives'] and len(argv) == 1:
        print('\n'.join(r['archive'] for r in rows()))
    elif argv[:1] == ['get'] and len(argv) == 3:
        print(get(argv[1], argv[2]))
    elif argv[:1] == ['index'] and len(argv) == 2:
        sys.stdout.write(index(argv[1]))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
