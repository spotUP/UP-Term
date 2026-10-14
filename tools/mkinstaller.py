#!/usr/bin/env python3
"""Assemble one package's Install, Files/install.dos or Uninstall from its template.

  python3 tools/mkinstaller.py <template> <package id> [NAME=value ...] > <out>

A template line `; @include <path>` (path from the repo root) is replaced by that
file, and every @NAME@ by its value, in the template and in what it includes. The
values come from the package's row in dist/packages.txt (tools/pkgs.py), from
dist/values/<id>.txt, and from the command line. The Installer 43.3 has no include
of its own that these scripts could use (kit-split plan D4), so the kit's scripts are
assembled when `make dist` builds them. A @NAME@ with no value, or an include that
includes itself, stops the tool.
"""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import kitsize, pkgs

INC = re.compile(r'(?m)^; @include (\S+)[ \t]*\n')


def sub(text, values):
    return re.sub(r'@([A-Z0-9]+)@', lambda m: values.get(m.group(1), m.group(0)), text)


def assemble(path, values, stack=()):
    """The template with each ; @include line replaced by its file (values
    substituted first, so a path may hold @ID@), and every @NAME@ by its value."""
    path = pathlib.Path(path)
    if path in stack:
        raise SystemExit('mkinstaller: %s includes itself' % path)
    text = sub(path.read_text(encoding='latin-1'), values)
    text = INC.sub(lambda m: assemble(ROOT / m.group(1), values, stack + (path,)), text)
    left = sorted(set(re.findall(r'@[A-Z0-9]+@', text)))
    if left:
        raise SystemExit('mkinstaller: %s: no value for %s' % (path, ' '.join(left)))
    return text


def handed_search(rows):
    """The Search lines of the base's record part (dist/common/record-legacy.dos): one for each
    path the base hands to an add-on archive, setting $uplegacy when the old manifest names it."""
    paths = sorted({p for r in rows if r['id'] != 'base' and r['handed'] != '-' for p in r['handed'].split(',')})
    # Search matches a substring: Files/Claude finds Files/Claude... (an old Files/ClaudeCode too)
    paths = [p for p in paths if not any(q != p and p.startswith(q) for q in paths)]
    out = []
    for path in paths:
        out += ['    Search >NIL: UP-Term:MANIFEST " %s"' % path,
                '    If NOT WARN', '      Set uplegacy YES', '    EndIf']
    return '\n'.join(out)


def addon_uninstall(rows):
    """The base Uninstall's lines that run each add-on's own Uninstall (the copy its Install kept in its
    record drawer under UP-Term:pkg); a standalone add-on (record in ENVARC:) is not run."""
    out = []
    for r in rows:
        if r['id'] != 'base' and r['record'].startswith('UP-Term:pkg/'):
            u = r['record'] + '/Uninstall'
            out += ['If EXISTS ' + u, '  Execute ' + u, 'EndIf']
    return '\n'.join(out)


def read_values(pkg_id, extra=()):
    """The @NAME@ values of one package."""
    row = {r['id']: r for r in pkgs.rows()}.get(pkg_id)
    if row is None:
        raise SystemExit('mkinstaller: no package %r in dist/packages.txt' % pkg_id)
    record = row['record']
    values = {
        'ID': row['id'], 'ARCHIVE': row['archive'], 'VERSION': row['version'],
        'NEEDSBASE': row['needs-base'], 'IXEMUL': row['ixemul'], 'RECORD': record,
        'HANDED': row['handed'],
        'OPTS': 'ENVARC:up-term/opts' if row['id'] == 'base' else record.rstrip(':/') + '/opts',
        # the record drawer with its separator (UP-Term: or UP-Term:pkg/UP-Term-nvim/)
        'RECORDP': record if record.endswith((':', '/')) else record + '/',
        # first lines of a package's record part and its Uninstall's data question: dist/common/ fragments
        'RECORDPRE': 'none.dos' if record.endswith(':') else 'record-addon.dos',
        'PKGDIR': record.rsplit('/', 1)[0] if '/' in record else record,
        'ADDONVAR': 'none.dos',
        # a stand-alone archive (needs no base, packages.txt): its record is in ENVARC:, it is installed and
        # updated with no UP-Term: assign, and DESTTEXT is what its pages call the place it installs to
        'STANDALONE': 'YES' if row['id'] != 'base' and row['needs-base'] == '-' else 'NO',
        'DESTTEXT': '',
        # may an Update take over the files the single kit installed (UP-Term:pkg/legacy, kit-split D7)
        'LEGACY': 'YES',
        'CHECKFRAG': 'check-standalone.dos' if row['id'] != 'base' and row['needs-base'] == '-' else 'check.dos',
        'HANDEDSEARCH': handed_search(pkgs.rows()) if row['id'] == 'base' else '',
        'ADDONUNINSTALL': addon_uninstall(pkgs.rows()) if row['id'] == 'base' else '',
    }
    if row['needs-base'] != '-':
        values['BASEMAJOR'], values['BASEMINOR'] = row['needs-base'].split('.')
    # the sizes of the staged kit (tools/kitsize.py): what the free-space check and the progress bar use. With no
    # staged kit (the host tests of the script language) every part counts its minimum.
    values.update({k: str(v) for k, v in kitsize.sizes(ROOT / 'build/dist' / row['archive'], pkg_id).items()})
    f = ROOT / 'dist/values' / (pkg_id + '.txt')
    lines = f.read_text(encoding='latin-1').splitlines() if f.exists() else []
    for line in list(lines) + list(extra):
        if line.strip() and not line.startswith('#'):
            k, _, v = line.partition('=')
            values[k.strip()] = v
    return values


def build(template, pkg_id, extra=()):
    return assemble(pathlib.Path(template), read_values(pkg_id, extra))


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    sys.stdout.buffer.write(build(sys.argv[1], sys.argv[2], sys.argv[3:]).encode('latin-1'))
