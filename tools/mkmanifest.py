#!/usr/bin/env python3
"""mkmanifest.py -- the kit's Files/MANIFEST, for the Installer's update.

  python3 tools/mkmanifest.py <kit drawer> [dist/parts.txt] [Files/install.dos]

One line per file of the kit, sorted by path (bytes):
  <part> <size> <crc32, 8 hex digits> <kit path>
<part> is the install.dos part that installs the file (dist/parts.txt, the longest
kit path that matches; "-" for none). And one line per part of install.dos:
  <part> 0 <crc32 of its text> install.dos#<part>
("all" for the text before the first part: every part reads it), so a part whose
script changed runs again. The Amiga compares two manifests (upupdate) and hashes
nothing itself. A kit file no line of parts.txt claims stops `make dist`."""
import pathlib, re, sys, zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SKIP = {"Files/MANIFEST"}


def read_parts(text):
    """[(part, kit path, drawer or None)] from parts.txt: columns are separated by two or more spaces
    (a path can hold one: Files/UP-Term Prefs)."""
    out = []
    for line in text.splitlines():
        line = line.rstrip()
        if not line or line.startswith('#'):
            continue
        f = re.split(r'\s{2,}', line)
        if len(f) not in (2, 3):
            raise SystemExit('parts.txt: %r: want <part> <kit path> [<drawer>]' % line)
        out.append((f[0], f[1], f[2] if len(f) == 3 else None))
    return out


def part_of(path, parts):
    """The (part, prefix, drawer) of the longest kit path in parts that is path or a drawer holding it."""
    best = None
    for p in parts:
        if path == p[1] or path.startswith(p[1] + '/'):
            if best is None or len(p[1]) > len(best[1]):
                best = p
    return best


def sections(dos):
    """[(part, text)] of install.dos: "all" for the text before the first Lab, then each Lab's text."""
    chunks = re.split(r'(?m)^Lab (\S+)[ \t]*\n', dos)
    return [('all', chunks[0])] + list(zip(chunks[1::2], chunks[2::2]))


def manifest(kit, parts, dos):
    lines, missing = [], []
    for f in sorted(kit.rglob('*')):
        if not f.is_file():
            continue
        rel = f.relative_to(kit).as_posix()
        if rel in SKIP:
            continue
        p = part_of(rel, parts)
        if p is None:
            missing.append(rel)
            continue
        data = f.read_bytes()
        lines.append((rel.encode('latin-1'), '%s %d %08x %s' % (p[0], len(data), zlib.crc32(data), rel)))
    if missing:
        raise SystemExit('mkmanifest: no line of dist/parts.txt claims %s' % ', '.join(missing[:8]))
    for name, text in sections(dos):
        rel = 'install.dos#' + name
        lines.append((rel.encode('latin-1'), '%s 0 %08x %s' % (name, zlib.crc32(text.encode('latin-1')), rel)))
    return '\n'.join(l for _, l in sorted(lines)) + '\n'


def main(a):
    kit = pathlib.Path(a[0])
    parts = read_parts(pathlib.Path(a[1] if len(a) > 1 else ROOT / 'dist/parts.txt').read_text())
    dos = pathlib.Path(a[2] if len(a) > 2 else kit / 'Files/install.dos').read_text(encoding='latin-1')
    (kit / 'Files/MANIFEST').write_text(manifest(kit, parts, dos), encoding='latin-1')


if __name__ == '__main__':
    main(sys.argv[1:])
