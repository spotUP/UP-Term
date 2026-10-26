#!/usr/bin/env python3
"""Contributor packages: the vtcon rows of one upterm-ports recipe, generated from the recipe.

  python3 tools/pkg_rows.py emit  <pkg> --ports DIR [--out DIR | --write]
  python3 tools/pkg_rows.py check <pkg> --ports DIR

The recipe (DIR/pkgs/<pkg>/recipe.mk) is read with upterm-ports' own tools/recipe-var.sh (`make print-<pkg>_<FIELD>`):
_FEED_ID, _SUMMARY, _FEED_MIN_BASE, _BINS, _DATA, _LICENSE, _VERSION. A recipe without _FEED_ID is a base-kit
program (tree): no rows. From the fields emit makes
  the dist/packages.txt row, the dist/slim.txt rows, the six files of dist/addons/<pkg>/ (templates in
  tools/pkg_rows_tpl/). Nothing else: the Makefile targets (dist-<id>, KIT_<id>) loop over dist/slim.txt and
  tools/pkgs.py reads the summary from the recipe, so a package touches no shared code (plan CP3).
With --write the rows are appended to the tables and the addons drawer is created; default is a scratch dir.
`check` regenerates and diffs against the committed rows, exits 1 naming the file and row that drifted.
What the recipe cannot say (prose of the Installer, README, the man page row) comes from tools/pkg_rows_prose/<pkg>.json
(or --prose FILE; key man lists the man page rows) and falls back to text built from _SUMMARY; --man PATH adds a slim row,
--pkg-version, --base-version, --ixemul set the packages.txt columns (defaults 1.0, 1.0, 80).
"""
import argparse, json, os, pathlib, subprocess, sys, tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import pkgs, slim

ROOT = pathlib.Path(__file__).resolve().parents[1]
TPL = ROOT / 'tools/pkg_rows_tpl'
PROSE = ROOT / 'tools/pkg_rows_prose'
ADDON_FILES = ('install.dos', 'Install.installer', 'LICENSES.txt', 'parts.txt', 'README.txt', 'uninstall-body.dos')
WIDTHS = (8, 11, 9, 12, 8, 23, 54)  # packages.txt column widths of the committed rows


def read_field(ports, pkg, field):
    """Recipe variable <pkg>_<field>: upterm-ports' recipe-var.sh `var`, the same make print-% every tool uses."""
    r = subprocess.run(['sh', '-c', '. "$ROOT/tools/recipe-var.sh"; var "$P" "$F"'],
                       env=dict(os.environ, ROOT=str(ports), P=pkg, F=field),
                       capture_output=True, text=True, check=True)
    return r.stdout.strip()


def load_prose(pkg, path=None):
    p = pathlib.Path(path) if path else PROSE / (pkg + '.json')
    return json.loads(p.read_text(encoding='utf-8')) if p.exists() else {}


def generate(pkg, ports, a):
    """{'packages': row, 'slim': [rows], 'addons': {name: text}} or None (base-kit program)."""
    fid = read_field(ports, pkg, 'FEED_ID')
    if not fid:
        return None
    summary = read_field(ports, pkg, 'SUMMARY')
    if not summary:
        raise SystemExit('pkg_rows: %s has _FEED_ID but no _SUMMARY' % pkg)
    bins = read_field(ports, pkg, 'BINS').split()
    data = read_field(ports, pkg, 'DATA').split()
    lic = read_field(ports, pkg, 'LICENSE')
    minbase = read_field(ports, pkg, 'FEED_MIN_BASE') or a.base_version
    pr = load_prose(pkg, a.prose)
    paths = bins + data + list(pr.get('man', [])) + list(a.man) + ['licenses/' + pkg]
    slim_rows = ['%s  %s' % (pkg, p) for p in paths]
    handed = ','.join('Files/userland/' + p for p in bins + data)
    cols = (pkg, fid, a.pkg_version, minbase, a.ixemul, 'UP-Term:pkg/' + fid, handed)
    row = ''.join(c.ljust(max(w, len(c) + 2)) for c, w in zip(cols, WIDTHS)) + 'upterm-ports,ixemul-vtcon'
    name = pr.get('name', pkg)
    cmd = pathlib.PurePosixPath(bins[0]).name if bins else name
    about = pr.get('about', summary)
    cc = pr.get('copy_comment', ['; %s: %s' % (name, summary)])
    welcome = pr.get('welcome', [summary])
    sub = {'ID': fid, 'PKG': pkg, 'PKGUP': pkg.upper(), 'NAME': name, 'CMD': cmd, 'ABOUT': about,
           'COPY_COMMENT': '\n'.join(cc) + '\n', 'TITLE': pr.get('title', ' '.join(name.upper())),
           'WELCOME': ''.join('    "%s\\n"\n' % l for l in welcome),
           'WHY': pr.get('why', 'Copying %s to UP-Term.' % name),
           'COPYROW': 'copy-%s' % pkg + ' ' * max(2, 19 - len('copy-' + pkg)) + 'Files/userland'}
    add = {}
    for f in ('install.dos', 'Install.installer', 'parts.txt', 'uninstall-body.dos'):
        t = (TPL / (f + '.tpl')).read_text(encoding='utf-8')
        for k, v in sub.items():
            t = t.replace('{{%s}}' % k, v)
        add[f] = t
    contents = pr.get('readme_contents', summary)
    use = pr.get('readme_use', 'In an UP-Term window, in vsh:\n  %s' % cmd)
    add['README.txt'] = (
        '%s: %s\n\nContents: %s\nLicence: %s; see LICENSES.txt.\n\n'
        'Needs up-term.lha installed first (version %s or newer). %s\n\n'
        'Install: run Install (Installer) or, with upkg, install %s. Uninstall takes the package out\n'
        'again; the UP-Term Uninstall runs it first.\n'
        % (fid, pr.get('readme_title', summary), contents, lic, minbase, use, fid))
    label = pr.get('part_label', name)
    lcol = '%s (licenses/%s)' % (lic, pkg)
    src = pr.get('source', 'upterm-ports (pkgs/%s)' % pkg)
    w1, w2, w3 = max(len(label), 4) + 1, max(len(lcol), 7) + 1, len(src) - 7
    ln = lambda x, y, z: '%s  %s  %s\n' % (x.ljust(w1), y.ljust(w2), z)
    add['LICENSES.txt'] = (
        '%s: parts, licences and where the source is\n\n' % fid + ln('Part', 'Licence', 'Source')
        + ln('-' * w1, '-' * w2, '-' * w3) + ln(label, lcol, src)
        + '\nNotes\n- The complete source with UP-Term\'s patches and build recipes is up-term-src.lha, published next to\n'
          '  up-term.lha. Its row of SOURCES.txt is in UP-Term:licenses/%s/SOURCES.txt.\n' % pkg)
    return {'packages': row, 'slim': slim_rows, 'addons': add}


def table_row(pkg, root):
    for line in (root / 'dist/packages.txt').read_text(encoding='utf-8').splitlines():
        if line and not line.startswith('#') and line.split()[0] == pkg:
            return line
    return None


def emit(pkg, g, out, write, root):
    if write:
        if table_row(pkg, root) or any(i == pkg for i, _ in slim.rows(root / 'dist/slim.txt')):
            raise SystemExit('pkg_rows: %s already has rows; refusing to overwrite (Replaces decision is the owner\'s)' % pkg)
        for rel, text in (('dist/packages.txt', g['packages'] + '\n'), ('dist/slim.txt', '\n'.join(g['slim']) + '\n')):
            with open(root / rel, 'a', encoding='utf-8') as f:
                f.write(text)
        out = root / 'dist/addons' / pkg
    else:
        (out / 'dist').mkdir(parents=True, exist_ok=True)
        (out / 'dist/packages.row').write_text(g['packages'] + '\n', encoding='utf-8')
        (out / 'dist/slim.rows').write_text('\n'.join(g['slim']) + '\n', encoding='utf-8')
        out = out / 'dist/addons' / pkg
    out.mkdir(parents=True, exist_ok=True)
    for n, t in g['addons'].items():
        (out / n).write_text(t, encoding='utf-8')


def check(pkg, g, root):
    bad = []
    got = table_row(pkg, root)
    if got is None or got != g['packages']:
        bad.append('dist/packages.txt: row %s: committed %r, generated %r' % (pkg, got, g['packages']))
    have = ['%s  %s' % (i, p) for i, p in slim.rows(root / 'dist/slim.txt') if i == pkg]
    for r in g['slim']:
        if r not in have:
            bad.append('dist/slim.txt: row missing: %s' % r)
    for r in have:
        if r not in g['slim']:
            bad.append('dist/slim.txt: row not generated: %s' % r)
    for n, t in g['addons'].items():
        p = root / 'dist/addons' / pkg / n
        if not p.exists() or p.read_text(encoding='utf-8') != t:
            bad.append('dist/addons/%s/%s: differs from the generated file' % (pkg, n))
    return bad


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('mode', choices=('emit', 'check'))
    ap.add_argument('pkg')
    ap.add_argument('--ports', required=True)
    ap.add_argument('--root', default=str(ROOT), help='the vtcon tree whose tables are written or checked')
    ap.add_argument('--out')
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--prose')
    ap.add_argument('--man', action='append', default=[])
    ap.add_argument('--pkg-version', default='1.0')
    ap.add_argument('--base-version', default='1.0')
    ap.add_argument('--ixemul', default='80')
    a = ap.parse_args(argv)
    ports, root = pathlib.Path(a.ports), pathlib.Path(a.root)
    g = generate(a.pkg, ports, a)
    if g is None:
        print('pkg_rows: %s has no _FEED_ID: a base-kit program, no feed rows' % a.pkg)
        return 0
    if a.mode == 'check':
        bad = check(a.pkg, g, root)
        for b in bad:
            print('[DRIFT] ' + b)
        return 1 if bad else 0
    out = pathlib.Path(a.out) if a.out else pathlib.Path(tempfile.mkdtemp(prefix='pkg_rows-'))
    emit(a.pkg, g, out, a.write, root)
    print('pkg_rows: %s -> %s' % (a.pkg, root if a.write else out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
