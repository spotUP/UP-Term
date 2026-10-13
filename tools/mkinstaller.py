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
import pkgs

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
    }
    if row['needs-base'] != '-':
        values['BASEMAJOR'], values['BASEMINOR'] = row['needs-base'].split('.')
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
