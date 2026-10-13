#!/usr/bin/env python3
"""kitsize.py -- how much room each part of a staged kit takes on an Amiga disk (K7).

  python3 tools/kitsize.py <package id>      the @KB...@ values of the package, one NAME=value a line

tools/mkinstaller.py puts these values into the Install scripts when `make dist` assembles them, so the
free-space check and the progress bar use the kit's real sizes, not numbers typed in once and left to
age (the base's guess of 2026-10-07 said 17 MB for a kit that had grown past 26 MB, and an Install onto a
106 MB System: partition died half-way with "Error in writing - disk is full").

A part's size is the sum, over the kit files dist/parts.txt (or the add-on's parts.txt) gives to that part,
of what the file takes on disk: a header block, its data blocks (the worst case, OFS: 488 bytes of data in
a 512-byte block) and one extension block for each 72 data blocks; plus one block for each drawer. Rounded
up to KB, never under FLOOR_KB (a part with no kit files still makes drawers and writes small files).

Values: KB<PART> for every part (the name upper-cased, the hyphens gone: copy-userland is KBCOPYUSERLAND),
KBTOTAL (everything the package installs, record part included) and, for the base, one for each option
whose files only some choices copy (OPTIONS) -- the part's value then leaves those files out.
"""
import fnmatch, math, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkmanifest, pkgs

BLOCK, DATA, EXT = 512, 488, 72
FLOOR_KB = 20
# parts whose files replace a library that is kept as <name>.orig: the room is needed twice
BACKED_UP = {'libs': 2, 'ixemul': 2}
# The base's options: the kit files (paths under Files/, patterns for the version in a drawer's name) that
# an option copies. ('all', [...]): every match is copied; ('one', [...]): the matches are alternatives, of
# which Install copies one (the CPU's build): the largest counts. tests/test_dist_kitsize.py holds every
# name here to a Copy line of install.dos.
OPTIONS = {
    'KBSSH': [('all', ['net/bebbossh-*/bebbossh', 'net/bebbossh-*/bebboscp', 'net/bebbossh-*/bebbosshkeygen',
                       'net/bebbossh-*/bebbosshd']),
              ('one', ['net/bebbossh-*/libcryptossh.library0?0'])],
    'KBBEBBOGET': [('all', ['net/bebboget-*/bebboget', 'net/bebboget-*/installcerts'])],
    'KBCURL': [('one', ['net/curl-*/curl.0?0'])],
    'KBAMIAGENT': [('one', ['net/amiagent-*/amiagent', 'net/amiagent-*/amiagent.020'])],
    'KBWASABI': [('all', ['wasabi'])],
}
# kit files no Install copies, whatever the options: the network tools' licences and sources
NOT_COPIED = ['net/*/*-src.tar.bz2', 'net/*/COPYING', 'net/*/SOURCE.txt']


def file_bytes(size):
    """What a file of `size` bytes takes on disk."""
    blocks = -(-size // DATA)
    return (1 + blocks + max(0, -(-blocks // EXT) - 1)) * BLOCK


def kb(nbytes):
    return -(-nbytes // 1024)


def tree(files_dir):
    """[(relative path under Files/, is a drawer, size)] for everything under the kit's Files drawer."""
    out = []
    for p in sorted(pathlib.Path(files_dir).rglob('*')):
        rel = p.relative_to(files_dir).as_posix()
        out.append((rel, p.is_dir(), 0 if p.is_dir() else p.stat().st_size))
    return out


def matches(rel, patterns):
    return any(fnmatch.fnmatch(rel, pat) or rel.startswith(pat + '/') for pat in patterns)


def option_files(files, name):
    """The kit files option `name` copies, as [(rel, size)] (see OPTIONS)."""
    out = []
    for how, pats in OPTIONS[name]:
        found = [(r, s) for r, d, s in files if not d and matches(r, pats)]
        if how == 'one':
            found = sorted(found, key=lambda f: f[1])[-1:]
        out += found
    return out


def parts_file(pkg_id):
    return ROOT / ('dist/parts.txt' if pkg_id == 'base' else 'dist/addons/%s/parts.txt' % pkg_id)


def sizes(kit, pkg_id):
    """{value name: KB} of the staged kit (a drawer holding Files/) for package pkg_id."""
    parts = mkmanifest.read_parts(parts_file(pkg_id).read_text(encoding='utf-8'))
    entries = tree(pathlib.Path(kit) / 'Files')
    options = {n: option_files(entries, n) for n in OPTIONS} if pkg_id == 'base' else {}
    # what a part's own value leaves out: the options' files (counted in their values) and what no Install copies
    left_out = {r for n in options for _, pats in OPTIONS[n] for r, d, _ in entries if not d and matches(r, pats)}
    left_out |= {r for r, d, s in entries if matches(r, NOT_COPIED)}
    wasabi = [(r, d, s) for r, d, s in entries if matches(r, ['wasabi'])] if options else []
    left_out |= {r for r, _, _ in wasabi}
    bytes_of = {}
    for rel, is_dir, size in entries:
        if 'Files/' + rel in mkmanifest.SKIP:
            continue        # written after the scripts, from the finished kit
        hit = mkmanifest.part_of('Files/' + rel, parts)
        if hit is None and is_dir:
            continue        # a drawer no line names: its files are counted where they belong
        if hit is None:
            sys.exit('kitsize: Files/%s: no line of parts.txt claims it' % rel)
        if hit[0] == '-' or rel in left_out:
            continue
        bytes_of[hit[0]] = bytes_of.get(hit[0], 0) + (BLOCK if is_dir else file_bytes(size))
    values = {}
    for part in {p[0] for p in parts if p[0] != '-'}:
        values['KB' + part.upper().replace('-', '')] = max(FLOOR_KB, kb(bytes_of.get(part, 0)) * BACKED_UP.get(part, 1))
    for name, fs in options.items():
        n = sum(file_bytes(s) for _, s in fs)
        if name == 'KBWASABI':
            n = sum(BLOCK if d else file_bytes(s) for _, d, s in wasabi)
        values[name] = max(FLOOR_KB, kb(n))
    # an add-on installs all of its parts; the base's total depends on the choices and is summed by its Install
    values['KBTOTAL'] = sum(v for k, v in values.items() if k not in OPTIONS)
    return values


def main(argv):
    if len(argv) != 1:
        sys.exit(__doc__)
    row = {r['id']: r for r in pkgs.rows()}.get(argv[0]) or sys.exit('kitsize: no package %r' % argv[0])
    for k, v in sorted(sizes(ROOT / 'build/dist' / row['archive'], argv[0]).items()):
        print('%s=%d' % (k, v))


if __name__ == '__main__':
    main(sys.argv[1:])
