#!/usr/bin/env python3
"""The package table dist/packages.txt, for the Makefile, mkinstaller, mkmanifest and the tests.

  python3 tools/pkgs.py names              ids, one a line
  python3 tools/pkgs.py archives           archive names, one a line
  python3 tools/pkgs.py get <id> <column>  one value ("-" is printed as it is)
  python3 tools/pkgs.py prune <dir>        delete every <dir>/*.lha that dist/packages.txt does not list
  python3 tools/pkgs.py index <dir>        upkg's index of the archives in <dir>
  python3 tools/pkgs.py ipk <build>        one .ipk per package into <build>/ipk (from build/dist/<archive>)
  python3 tools/pkgs.py feed <build>       <build>/ipk/Packages and Packages.gz of those .ipk files
"""
import gzip, hashlib, io, os, pathlib, re, sys, tarfile

import slim

ROOT = pathlib.Path(__file__).resolve().parents[1]
TABLE = ROOT / 'dist/packages.txt'
RENAMES = ROOT / 'dist/package-renames.txt'
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


def renames(path=RENAMES):
    """[(old, new)] of dist/package-renames.txt ("old new" a line, # comments)."""
    out = []
    for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if line and not line.startswith('#'):
            f = line.split()
            if len(f) != 2:
                raise SystemExit('package-renames.txt: %r: want "old new"' % line)
            out.append((f[0], f[1]))
    return out


def old_names(archive, path=RENAMES):
    """The names the package <archive> had before (empty for a package that was never renamed)."""
    return [o for o, n in renames(path) if n == archive]


def old_record(r, path=RENAMES):
    """[(old record, new record)] of a row: its record drawer spelled with each old name; a record that
    does not end in the archive name (the base's UP-Term:) has none."""
    rec = r['record'].rstrip('/')
    if not rec.endswith(r['archive']):
        return []
    return [(rec[:-len(r['archive'])] + o, rec) for o in old_names(r['archive'], path)]


def rename_lines(pairs):
    """AmigaDOS lines that move a record drawer left by the old Installer to its new name; nothing is
    done when the new one is already there."""
    out = []
    for old, new in pairs:
        out += ['If EXISTS ' + old, '  If NOT EXISTS ' + new, '    Rename >NIL: %s %s' % (old, new), '  EndIf', 'EndIf']
    return '\n'.join(out)


def lha_rows(path=TABLE):
    """The rows published as an .lha next to up-term.lha. The packages of dist/slim.txt (git, perl, vim, bitchx, vbcc)
    are upkg-only (owner 2026-10-19): the .ipk feed ships them, so no .lha, no index line, no Installer
    checkbox and no Aminet readme."""
    upkg_only = slim.ids()
    return [r for r in rows(path) if r['id'] not in upkg_only]


def get(pkg_id, column, path=TABLE):
    if column not in COLUMNS:
        raise SystemExit('pkgs: no column %r (have %s)' % (column, ' '.join(COLUMNS)))
    for r in rows(path):
        if r['id'] == pkg_id:
            return r[column]
    raise SystemExit('pkgs: no package %r' % pkg_id)


SOURCE_ARCHIVE = 'up-term-src'   # the source archive (Makefile dist-src): published next to the base, not a package row


def prune(build, path=TABLE):
    """Delete every <build>/*.lha that is not an archive of the table (lha_rows) or the source archive: what a
    renamed or dropped package left behind. The name is compared as spelled (a case-insensitive disk keeps the
    spelling the archive was written with, so a capitalised base archive is stale). Returns the names deleted."""
    keep = {r['archive'] + '.lha' for r in lha_rows(path)} | {SOURCE_ARCHIVE + '.lha'}
    gone = sorted(n for n in os.listdir(build) if n.endswith('.lha') and n not in keep)
    for n in gone:
        os.unlink(os.path.join(build, n))
    return gone


def index(build, path=TABLE):
    """upkg's index (upkg plan D1): a line per archive, fields one space apart:
    id archive version needs-base record size sha256 of <build>/<archive>.lha.
    Released next to the archives; an archive missing from <build> is refused."""
    out = ['# upkg index 1: id archive version needs-base record size sha256']
    for r in lha_rows(path):
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
    'dopus': 'Directory Opus 5 5.104 (the 68k build for the CPU), through its own installer',
    'git': 'git 2.56.0 in UP-Term:bin and UP-Term:share/git-core',
    'perl': 'Perl 5.42.3 in UP-Term:bin',
    'vim': 'Vim 9.2.1202 in UP-Term:bin and UP-Term:share/vim',
    'bitchx': 'BitchX IRC client in UP-Term:bin and its IRCLIB in UP-Term:lib/bx',
    'uhc': 'the UHC tools (aget, UHCSearch and the get/search scripts) in UP-Term:uhc',
    'vbcc': 'vbcc 0.9h C compiler for AmigaOS 68k in UP-Term:vbcc',
}
# Packages that never had another name (born as up-<x>, 2026-10-25 up-bitchx): they have no line in
# dist/package-renames.txt and no old record to migrate. tests/test_dist_packages.py holds both facts.
# Derived: every slim package (dist/slim.txt) with no old name.
# up-dopus is no slim package: born as up-dopus (2026-10-26), the one hand-written package without an old record (up-uhc, 2026-10-26, the second).
NEW_PACKAGES = tuple('up-' + i for i in slim.ids() if not old_names('up-' + i)) + ('up-dopus', 'up-uhc')
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
    # a record without its Uninstall (an install from before the copy was fixed) must not block an upgrade
    # or a removal: warn and go on, so opkg still replaces the package
    # the Uninstall deletes its own record drawer, and AmigaDOS cannot delete a file a running script holds
    # open: it runs from a copy in T:, so the record drawer holds no open file when the Delete reaches it
    pre = ('#!/bin/sh\n# written by tools/pkgs.py ipk: runs a copy of the Uninstall the install left in its record\n'
           'if [ -f %(u)s ]; then\n    Copy %(u)s %(c)s CLONE QUIET\n    Execute %(c)s\n    Delete >NIL: %(c)s QUIET\nelse\n'
           '    echo "warning: %(u)s does not exist, nothing to uninstall; continuing" >&2\nfi\nexit 0\n'
           % {'u': '%s%sUninstall' % (rec, sep), 'c': 'T:%s.Uninstall' % r['archive']})
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
    # From ./T/upkg-stage down, never ./T itself: /T is the T: assign, which always exists, and an
    # entry for it makes opkg try to create /T and warn "Can't create '/T' (errno=5)" on every install.
    for i in range(2, len(parts) + 1):
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


def _rename_fields(r):
    """Replaces, Conflicts and Provides of a renamed package: opkg removes the old package, running
    its prerm, before it installs this one, and anything that depends on the old name is still met."""
    olds = old_names(r['archive'])
    if not olds:
        return []
    j = ', '.join(olds)
    return ['Replaces: ' + j, 'Conflicts: ' + j, 'Provides: ' + j]


def _interactive_fields(build, r):
    """['UP-Term-Interactive: yes'] for an archive with install questions (its Files/questions file), else [].
    upkg browse reads it from the feed's Packages list and runs such a package in the plain window, not in
    a programbox. One source: the questions file."""
    if (pathlib.Path(build) / 'dist' / r['archive'] / 'Files/questions').is_file():
        return ['UP-Term-Interactive: yes']
    return []


def build_ipk(r, build):
    """The .ipk bytes of one row from the staged tree <build>/dist/<archive>."""
    kit = pathlib.Path(build) / 'dist' / r['archive']
    if not (kit / 'Files/install.dos').is_file():
        raise SystemExit('pkgs ipk: %s is not staged: make dist' % kit)
    data_ents, size = _data_entries(kit, r['archive'])
    keys = question_keys(kit)
    need, post, pre = _scripts(r, keys)
    lines = ['Package: ' + r['archive'], 'Version: ' + r['version'], 'Architecture: ' + ARCH,
             'Maintainer: ' + MAINTAINER, 'Installed-Size: %d' % ((size + 1023) // 1024),
             'Description: ' + SUMMARY.get(r['id'], r['archive'])]
    lines[2:2] = _rename_fields(r)
    lines += _interactive_fields(build, r)
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
        b = ['Package: ' + r['archive'], 'Version: ' + r['version']] + _rename_fields(r) + _interactive_fields(build, r)
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
        print('\n'.join(r['archive'] for r in lha_rows()))
    elif argv[:1] == ['get'] and len(argv) == 3:
        print(get(argv[1], argv[2]))
    elif argv[:1] == ['prune'] and len(argv) == 2:
        for n in prune(argv[1]):
            print('dist: deleted stale %s/%s' % (argv[1], n))
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
