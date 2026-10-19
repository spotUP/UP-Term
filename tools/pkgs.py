#!/usr/bin/env python3
"""The package table dist/packages.txt, for the Makefile, mkinstaller, mkmanifest and the tests.

  python3 tools/pkgs.py names              ids, one a line
  python3 tools/pkgs.py archives           archive names, one a line
  python3 tools/pkgs.py get <id> <column>  one value ("-" is printed as it is)
  python3 tools/pkgs.py index <dir>        upkg's index of the archives in <dir>
  python3 tools/pkgs.py ipk <build>        one .ipk per package into <build>/ipk (from build/dist/<archive>)
  python3 tools/pkgs.py feed <build>       <build>/ipk/Packages and Packages.gz of those .ipk files
"""
import gzip, hashlib, io, os, pathlib, re, sys, tarfile

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


# ---- the .ipk feed (upkg plan P10.4) ----
# Architecture of every package: the one opkg arch the Amiga build has. opkg installs a package only
# when its Architecture is "all", "noarch" or an `arch` line of the conf (the kit's upkg.conf must carry
# `arch m68k-amigaos 10`). One string, because the binaries are 68020+ ixemul and nothing else runs them.
ARCH = 'm68k-amigaos'
MAINTAINER = 'UP-Term <https://github.com/spotUP/UP-Term>'
# Where an add-on's files wait: T:upkg-stage/<archive> (RAM, owner 2026-10-19). data.tar paths are
# relative to the dest root, and dist/upkg.conf has `dest root /`: ixemul reads /T/x as T:x, so
# ./T/upkg-stage/<archive>/... lands in T: with no second dest and no patch of the upkg binary.
STAGE = 'T/upkg-stage'
STAGE_DOS = 'T:upkg-stage'
SUMMARY = {
        'claude': 'Claude Code for the Amiga (C:Claude)',
    'pi': 'amiga-pi, the pi coding agent',
    'nvim': 'Neovim 0.12 in UP-Term:nvim',
    'python': 'Python 3.14 in SYS:Python3',
}
EPOCH = 0


def addons(path=TABLE):
    """The rows that become packages: every row but the base. The base is not a package (owner
    2026-10-19): it installs and updates from its own Installer, and upkg runs inside it."""
    return [r for r in rows(path) if r['record'] != 'UP-Term:']


def _scripts(r, keys=()):
    """(preinst, postinst, prerm) vsh text; preinst is None for an add-on with no needs-base. The package's own install logic is the only one (owner 2026-10-18):
    postinst Executes the archive's Files/install.dos, which `make dist` wrote and the Installer also
    runs; prerm Executes the Uninstall copy that install.dos left in the record drawer. keys: the
    archive's install questions (Files/questions, P10.6); postinst asks them with UP-Term:bin/upkg-ask
    (the package name is its third argument: the masthead's "NAME i/n")
    and hands each answer to install.dos as its keyword (KEY "value"), where the Installer's askstring
    answer goes too. A base without upkg-ask passes none, so install.dos keeps its own defaults."""
    st = '%s/%s' % (STAGE_DOS, r['archive'])
    rec = r['record'].rstrip('/')
    if keys:
        post = ('#!/bin/sh\n# written by tools/pkgs.py ipk: asks the archive\'s install questions, runs its own install script\n'
                '# with the answers, then drops the staged tree\n'
                'set --\nif [ -f UP-Term:bin/upkg-ask ] && vsh UP-Term:bin/upkg-ask %s/Files/questions %s/answers %s; then\n'
                '    . %s/answers\n    set -- %s\nfi\n'
                'Execute %s/Files/install.dos %s/Files "$@"\nrc=$?\nrm -rf %s\nexit $rc\n'
                % (st, st, r['archive'], st, ' '.join('%s "$%s"' % (k, k) for k in keys), st, st, st))
    else:
        post = ('#!/bin/sh\n# written by tools/pkgs.py ipk: runs the archive\'s own install script, then drops the staged tree\n'
                'Execute %s/Files/install.dos %s/Files\nrc=$?\nrm -rf %s\nexit $rc\n' % (st, st, st))
    sep = '' if rec.endswith(':') else '/'
    pre = ('#!/bin/sh\n# written by tools/pkgs.py ipk: the Uninstall copy the install left in its record\n'
           'Execute %s%sUninstall\n' % (rec, sep))
    need = None
    if r['needs-base'] != '-':
        # the refusal the retired vsh-script upkg made (same VERSIONS line, same older()): opkg aborts the
        # install when preinst exits non-zero, so the staged tree is never even unpacked
        need = ('#!/bin/sh\n# written by tools/pkgs.py ipk: refuse when the installed UP-Term is older than needs-base %(n)s\n'
                'older() {\n    awk -v a="$1" -v b="$2" \'BEGIN {\n'
                '        na = split(a, x, "."); nb = split(b, y, "."); n = na > nb ? na : nb\n'
                '        for (i = 1; i <= n; i++) { if (x[i] + 0 < y[i] + 0) exit 0; if (x[i] + 0 > y[i] + 0) exit 1 }\n'
                '        exit 1 }\'\n}\n'
                'base=\nif [ -f UP-Term:VERSIONS ]; then\n    base=$(head -n 1 UP-Term:VERSIONS | awk \'{ print $2 }\')\nfi\n'
                '[ -n "$base" ] || { echo "%(a)s needs UP-Term %(n)s, and UP-Term: has no install record" >&2; exit 1; }\n'
                'if older "$base" %(n)s; then\n'
                '    echo "%(a)s %(v)s needs UP-Term %(n)s; this is UP-Term $base: install the new UP-Term first" >&2\n    exit 1\nfi\n'
                % {'n': r['needs-base'], 'a': r['archive'], 'v': r['version']})
    return need, post, pre


def _tinfo(name, size=0, mode=0o644, kind=tarfile.REGTYPE, link=''):
    t = tarfile.TarInfo(name)
    t.size, t.mode, t.type, t.linkname = size, mode, kind, link
    t.mtime, t.uid, t.gid, t.uname, t.gname = EPOCH, 0, 0, 'root', 'root'
    return t


def _targz(entries):
    """Deterministic .tar.gz bytes of [(TarInfo, bytes|path|None)] in the order given."""
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode='w', format=tarfile.GNU_FORMAT) as tf:
        for ti, src in entries:
            if isinstance(src, bytes):
                tf.addfile(ti, io.BytesIO(src))
            elif src is None:
                tf.addfile(ti)
            else:
                with open(src, 'rb') as f:
                    tf.addfile(ti, f)
    out = io.BytesIO()
    with gzip.GzipFile(filename='', mode='wb', fileobj=out, compresslevel=9, mtime=0) as gz:
        gz.write(raw.getvalue())
    return out.getvalue()


def _data_entries(kit, archive):
    """Every entry of the staged tree build/dist/<archive>, sorted, under ./T/upkg-stage/<archive>/."""
    top = '%s/%s' % (STAGE, archive)
    ents, size = [], 0
    parts = top.split('/')
    for i in range(1, len(parts) + 1):
        ents.append((_tinfo('./' + '/'.join(parts[:i]), mode=0o755, kind=tarfile.DIRTYPE), None))
    for dirpath, dirs, files in os.walk(kit):
        dirs.sort()
        rel = os.path.relpath(dirpath, kit)
        base = top if rel == '.' else top + '/' + rel
        if rel != '.':
            ents.append((_tinfo('./' + base, mode=0o755, kind=tarfile.DIRTYPE), None))
        for fn in sorted(files):
            fp = os.path.join(dirpath, fn)
            name = './%s/%s' % (base, fn)
            if os.path.islink(fp):
                ents.append((_tinfo(name, mode=0o777, kind=tarfile.SYMTYPE, link=os.readlink(fp)), None))
                continue
            st = os.stat(fp)
            size += st.st_size
            ents.append((_tinfo(name, st.st_size, 0o755 if st.st_mode & 0o111 else 0o644), fp))
    return ents, size


def _ar(members):
    out = b'!<arch>\n'
    for name, data in members:
        out += b'%-16s%-12d%-6d%-6d%-8s%-10d`\n' % ((name + '/').encode(), EPOCH, 0, 0, b'100644', len(data))
        out += data + (b'\n' if len(data) % 2 else b'')
    return out


def ipk_name(r):
    return '%s_%s_%s.ipk' % (r['archive'], r['version'], ARCH)


def question_keys(kit):
    """The keys of <kit>/Files/questions (dist/upkg-ask's key|type|prompt|default|choices lines), in
    order; each must be a keyword of Files/install.dos's .KEY line, which postinst passes it as."""
    q = pathlib.Path(kit) / 'Files/questions'
    if not q.is_file():
        return []
    head = (pathlib.Path(kit) / 'Files/install.dos').read_text(encoding='latin-1').split('\n', 1)[0]
    words = {w.split('/')[0].upper() for w in head[len('.KEY'):].strip().split(',')} if head.upper().startswith('.KEY') else set()
    keys = []
    for line in q.read_text(encoding='latin-1').splitlines():
        if not line or line.startswith('#'):
            continue
        k = line.split('|', 1)[0]
        if not re.fullmatch(r'[A-Z][A-Z0-9_]*', k) or k not in words:
            raise SystemExit('pkgs ipk: %s: question %s is not a keyword of install.dos .KEY %s' % (q, k, sorted(words)))
        keys.append(k)
    return keys


def build_ipk(r, build):
    """The .ipk bytes of one row from the staged tree <build>/dist/<archive>."""
    kit = pathlib.Path(build) / 'dist' / r['archive']
    if not (kit / 'Files/install.dos').is_file():
        raise SystemExit('pkgs ipk: %s is not staged: make dist' % kit)
    data_ents, size = _data_entries(kit, r['archive'])
    need, post, pre = _scripts(r, question_keys(kit))
    lines = ['Package: ' + r['archive'], 'Version: ' + r['version'], 'Architecture: ' + ARCH,
             'Maintainer: ' + MAINTAINER, 'Installed-Size: %d' % ((size + 1023) // 1024),
             'Description: ' + SUMMARY.get(r['id'], r['archive'])]
    ctl = ('\n'.join(lines) + '\n').encode()
    ents = [(_tinfo('./', mode=0o755, kind=tarfile.DIRTYPE), None), (_tinfo('./control', len(ctl)), ctl)]
    if need:
        ents.append((_tinfo('./preinst', len(need), 0o755), need.encode()))
    ents += [(_tinfo('./postinst', len(post), 0o755), post.encode()), (_tinfo('./prerm', len(pre), 0o755), pre.encode())]
    control = _targz(ents)
    data = _targz([(_tinfo('./', mode=0o755, kind=tarfile.DIRTYPE), None)] + data_ents)
    return _ar([('debian-binary', b'2.0\n'), ('control.tar.gz', control), ('data.tar.gz', data)])


def ipk(build, path=TABLE):
    """Write one .ipk per row into <build>/ipk; byte-equal on a rebuild of the same staged tree."""
    out = pathlib.Path(build) / 'ipk'
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob('*.ipk'):
        old.unlink()
    names = []
    for r in addons(path):
        (out / ipk_name(r)).write_bytes(build_ipk(r, build))
        names.append(ipk_name(r))
    return names


def feed(build, path=TABLE):
    """Packages text (opkg format) of the .ipk files in <build>/ipk; one stanza a row."""
    out, blocks = pathlib.Path(build) / 'ipk', []
    for r in addons(path):
        f = out / ipk_name(r)
        if not f.is_file():
            raise SystemExit('pkgs feed: %s missing: python3 tools/pkgs.py ipk %s' % (f, build))
        data = f.read_bytes()
        b = ['Package: ' + r['archive'], 'Version: ' + r['version']]
        b += ['Architecture: ' + ARCH, 'Filename: ' + f.name, 'Size: %d' % len(data),
              'SHA256sum: ' + hashlib.sha256(data).hexdigest(),
              'Description: ' + SUMMARY.get(r['id'], r['archive'])]
        blocks.append('\n'.join(b) + '\n')
    return '\n'.join(blocks)


def write_feed(build, path=TABLE):
    text = feed(build, path).encode()
    out = pathlib.Path(build) / 'ipk'
    (out / 'Packages').write_bytes(text)
    with open(out / 'Packages.gz', 'wb') as f:
        with gzip.GzipFile(filename='', mode='wb', fileobj=f, compresslevel=9, mtime=0) as gz:
            gz.write(text)


def main(argv):
    if argv[:1] == ['names'] and len(argv) == 1:
        print('\n'.join(r['id'] for r in rows()))
    elif argv[:1] == ['archives'] and len(argv) == 1:
        print('\n'.join(r['archive'] for r in rows()))
    elif argv[:1] == ['get'] and len(argv) == 3:
        print(get(argv[1], argv[2]))
    elif argv[:1] == ['index'] and len(argv) == 2:
        sys.stdout.write(index(argv[1]))
    elif argv[:1] == ['ipk'] and len(argv) == 2:
        for n in ipk(argv[1]):
            print(n)
    elif argv[:1] == ['feed'] and len(argv) == 2:
        write_feed(argv[1])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
