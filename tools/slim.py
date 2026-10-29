#!/usr/bin/env python3
"""The slim base (P9): which paths of the ports' userland tree belong to a package, not to up-term.lha.

  python3 tools/slim.py ids                          package ids, one a line
  python3 tools/slim.py kits                         <id>=<archive> a line: the dist drawer of each package is
                                                     build/dist/<archive>, the packages.txt archive (not up-<id>)
  python3 tools/slim.py remove <userland>            take every package's paths (and SOURCES.txt rows) out of <userland>
  python3 tools/slim.py stage <id> <userland> <dest> copy the package's paths from <userland> into <dest>, same
                                                     relative paths; its SOURCES.txt row goes to licenses/<id>/SOURCES.txt
  python3 tools/slim.py paths <id> <userland>        the package's paths in <userland>, one a line
  python3 tools/slim.py uninstall <id>               the AmigaDOS Delete lines of the package's Uninstall

The list is dist/slim.txt, the only place that names the paths.
"""
import glob, os, pathlib, re, shutil, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TABLE = ROOT / 'dist/slim.txt'


def rows(path=TABLE):
    """[(id, pattern)] from the table."""
    out = []
    for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if line and not line.startswith('#'):
            f = re.split(r'\s{2,}|\s+', line)
            if len(f) != 2:
                raise SystemExit('slim.txt: %r: want <id> <path>' % line)
            out.append((f[0], f[1]))
    return out


def ids(path=TABLE):
    seen = []
    for i, _ in rows(path):
        if i not in seen:
            seen.append(i)
    return seen


def kits(path=TABLE):
    """[(id, archive)] for every package: its stage drawer is named after the packages.txt archive, which
    mkkit.py reads, and that is not always up-<id> (sqlite ships as up-sqlite3)."""
    import pkgs
    return [(i, pkgs.get(i, 'archive')) for i in ids(path)]


def patterns(pkg_id, path=TABLE):
    got = [p for i, p in rows(path) if i == pkg_id]
    if not got:
        raise SystemExit('slim: no package %r in %s' % (pkg_id, path))
    return got


def paths(pkg_id, userland, table=TABLE):
    """Relative paths in <userland> the package owns; a pattern that matches nothing is an error."""
    out = []
    for pat in patterns(pkg_id, table):
        hit = sorted(os.path.relpath(p, userland) for p in glob.glob(os.path.join(userland, pat)))
        if not hit:
            raise SystemExit('slim: %s: %s matches nothing in %s' % (pkg_id, pat, userland))
        out += hit
    return out


def _sources(userland):
    return pathlib.Path(userland) / 'SOURCES.txt'


def _row_of(pkg_id, text):
    return [l for l in text.splitlines(True) if l.split('\t', 1)[0] == pkg_id]


def remove(userland, table=TABLE):
    src = _sources(userland)
    text = src.read_text(encoding='utf-8')
    for i in ids(table):
        for rel in paths(i, userland, table):
            p = pathlib.Path(userland) / rel
            shutil.rmtree(p) if p.is_dir() and not p.is_symlink() else p.unlink()
        if not _row_of(i, text):
            raise SystemExit('slim: %s has no row in %s' % (i, src))
        text = ''.join(l for l in text.splitlines(True) if l not in _row_of(i, text))
    src.write_text(text, encoding='utf-8')


def stage(pkg_id, userland, dest, table=TABLE):
    dest = pathlib.Path(dest)
    for rel in paths(pkg_id, userland, table):
        s, d = pathlib.Path(userland) / rel, dest / rel
        d.parent.mkdir(parents=True, exist_ok=True)
        if s.is_dir() and not s.is_symlink():
            shutil.copytree(s, d, symlinks=True)
        else:
            shutil.copy2(s, d, follow_symlinks=False)
    row = _row_of(pkg_id, _sources(userland).read_text(encoding='utf-8'))
    if not row:
        raise SystemExit('slim: %s has no row in %s' % (pkg_id, _sources(userland)))
    (dest / 'licenses' / pkg_id).mkdir(parents=True, exist_ok=True)
    (dest / 'licenses' / pkg_id / 'SOURCES.txt').write_text(''.join(row), encoding='utf-8')


def uninstall(pkg_id, table=TABLE):
    """Delete lines for the package's paths: UPTREAL:<path> (the one real drawer of a multi-directory UP-Term:, assigned by the
    Uninstall's resolve-addon.dos), * written as the AmigaDOS #?."""
    lines = ['Delete >NIL: "UPTREAL:%s" ALL QUIET' % p.replace('*', '#?') for p in patterns(pkg_id, table)]
    return '\n'.join(lines)


def main(a):
    if a[:1] == ['ids'] and len(a) == 1:
        print('\n'.join(ids()))
    elif a[:1] == ['kits'] and len(a) == 1:
        print('\n'.join('%s=%s' % k for k in kits()))
    elif a[:1] == ['remove'] and len(a) == 2:
        remove(a[1])
    elif a[:1] == ['stage'] and len(a) == 4:
        stage(a[1], a[2], a[3])
    elif a[:1] == ['paths'] and len(a) == 3:
        print('\n'.join(paths(a[1], a[2])))
    elif a[:1] == ['uninstall'] and len(a) == 2:
        print(uninstall(a[1]))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
