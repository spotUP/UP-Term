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
import kitsize, pkgs, slim

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


def installer_rename(pairs):
    """The Installer-language form of pkgs.rename_lines, for the update probe that reads the old MANIFEST."""
    return '\n'.join('(if (and (exists "%s") (not (exists "%s"))) (run (cat "Rename >NIL: " "%s" " " "%s")))' % (o, n, o, n)
                     for o, n in pairs)


def addon_uninstall(rows):
    """The base Uninstall's lines that run each add-on's own Uninstall (the copy its Install kept in its
    record drawer under UP-Term:pkg); a standalone add-on (record in ENVARC:) is not run."""
    out = []
    for r in rows:
        if r['id'] != 'base' and r['record'].startswith('UP-Term:pkg/'):
            u = r['record'] + '/Uninstall'
            out += ['If EXISTS ' + u, '  Execute ' + u, 'EndIf']
    return '\n'.join(out)


def _addons(rows):
    upkg_only = slim.ids()      # git, perl, vim, bitchx, vbcc: upkg install, never a checkbox of the base Install
    return [r for r in rows if r['id'] != 'base' and r['id'] not in upkg_only]


def addon_find(rows):
    """The base Install's search: for every non-base row of packages.txt, set #ad<id>kind ("drawer",
    "lha" or "") and #ad<id>src (the Install script, or the lha) through P_ADFIND, and #adany."""
    out = []
    for r in _addons(rows):
        i = r['id']
        out += ['(set #adarc "%s")' % r['archive'], '(P_ADFIND)',
                '(set #ad%skind #adkind #ad%ssrc #adsrc)' % (i, i),
                '(if (<> #adkind "") (set #adany 1))']
    return '\n'.join(out)


def addon_choices(rows):
    """The choices of the add-on page: one per non-base row, saying where the archive was found."""
    return '\n'.join(
        '(cat "%s" (if (= #ad%skind "drawer") " (drawer found)" (if (= #ad%skind "lha") " (will unpack lha)" " (not found)")))'
        % (r['archive'], r['id'], r['id']) for r in _addons(rows))


def addon_run(rows):
    """The base Install's lines that run each ticked, found add-on (index = its place among the non-base rows)."""
    out = []
    for n, r in enumerate(_addons(rows)):
        i = r['id']
        out += ['(if (and (in #adpick %d) (<> #ad%skind ""))' % (n, i),
                '    (' , '        (set #adarc "%s" #adkind #ad%skind #adsrc #ad%ssrc)' % (r['archive'], i, i),
                '        (P_ADRUN)))']
    return '\n'.join(out)


def read_values(pkg_id, extra=()):
    """The @NAME@ values of one package."""
    row = {r['id']: r for r in pkgs.rows()}.get(pkg_id)
    if row is None:
        raise SystemExit('mkinstaller: no package %r in dist/packages.txt' % pkg_id)
    record = row['record']
    # @ARCHIVE@ is the name the script's texts show: the base's is the product UP-Term (its archive is up-term)
    values = {
        'ID': row['id'], 'ARCHIVE': 'UP-Term' if row['id'] == 'base' else row['archive'], 'VERSION': row['version'],
        'NEEDSBASE': row['needs-base'], 'IXEMUL': row['ixemul'], 'RECORD': record,
        'HANDED': row['handed'],
        'OPTS': 'ENVARC:up-term/opts' if row['id'] == 'base' else record.rstrip(':/') + '/opts',
        # the record drawer with its separator (UP-Term: or UP-Term:pkg/up-nvim/)
        'RECORDP': record if record.endswith((':', '/')) else record + '/',
        # first lines of a package's record part and its Uninstall's data question: dist/common/ fragments
        'RECORDPRE': 'none.dos' if record.endswith(':') else 'record-addon.dos',
        # where record.dos writes the MANIFEST and options: an add-on through $uptrec (set by its RECORDRESOLVE,
        # UPTREAL: on a multi-directory UP-Term:), which RECORDEND then unassigns; the base by its own names
        'RECORDW': (record if record.endswith((':', '/')) else record + '/') if record.endswith(':') else '$uptrec/',
        'OPTSW': ('ENVARC:up-term/opts' if row['id'] == 'base' else record.rstrip(':/') + '/opts')
                 if record.endswith(':') else '$uptrec/opts',
        'RECORDEND': 'none.dos' if record.endswith(':') else 'record-end-addon.dos',
        'PKGDIR': record.rsplit('/', 1)[0] if '/' in record else record,
        # where the record drawer is made: an add-on of the base resolves UP-Term: to one real drawer first
        # (a multi-directory assign creates in its first directory only), a stand-alone one has an absolute record
        'RECORDRESOLVE': 'resolve-addon.dos' if record.startswith('UP-Term:') else 'resolve-plain.dos',
        'PKGDIRREAL': 'UPTREAL:' + record[len('UP-Term:'):].rsplit('/', 1)[0] if record.startswith('UP-Term:') else '',
        'RECORDREAL': 'UPTREAL:' + record[len('UP-Term:'):] if record.startswith('UP-Term:') else '',
        'ADDONVAR': 'none.dos',
        # the base's Update reads the options back from the Amiga when the last install recorded none
        'OLDOPTS': 'oldopts.installer' if row['id'] == 'base' else 'none.dos',
        # a stand-alone archive (needs no base, packages.txt): its record is in ENVARC:, it is installed and
        # updated with no UP-Term: assign, and DESTTEXT is what its pages call the place it installs to
        'STANDALONE': 'YES' if row['id'] != 'base' and row['needs-base'] == '-' else 'NO',
        'DESTTEXT': '',
        # YES: the package ships a 68000 build and chooses it itself, so requirements.installer lets a 68000 pass
        'CPU68000': 'NO',
        # may an Update take over the files the single kit installed (UP-Term:pkg/legacy, kit-split D7)
        'LEGACY': 'YES',
        'CHECKFRAG': 'check-standalone.dos' if row['id'] != 'base' and row['needs-base'] == '-' else 'check.dos',
        'HANDEDSEARCH': handed_search(pkgs.rows()) if row['id'] == 'base' else '',
        'ADDONUNINSTALL': addon_uninstall(pkgs.rows()) if row['id'] == 'base' else '',
        # a package that was renamed (dist/package-renames.txt): its record drawer of the old name is renamed
        # first, by the check part (a Shell or upkg install), by the Installer's update probe, and by the base's
        # Uninstall for every add-on, so a record the old Installer wrote is found under the new name
        'RENAMERECORD': pkgs.rename_lines(pkgs.old_record(row)),
        'RENAMERECORDI': installer_rename(pkgs.old_record(row)),
        'RECORDRENAMES': pkgs.rename_lines([p for r in pkgs.rows() if r['id'] != 'base' for p in pkgs.old_record(r)])
                         if row['id'] == 'base' else '',
        'ADDONFIND': addon_find(pkgs.rows()) if row['id'] == 'base' else '',
        'ADDONCHOICES': addon_choices(pkgs.rows()) if row['id'] == 'base' else '',
        'ADDONRUN': addon_run(pkgs.rows()) if row['id'] == 'base' else '',
        'ADDONDEFAULT': str(2 ** len(_addons(pkgs.rows())) - 1) if row['id'] == 'base' else '',
    }
    if pkg_id in slim.ids():
        # the slim base (P9): the Delete lines of the paths dist/slim.txt gives the package
        values['SLIMDELETE'] = slim.uninstall(pkg_id)
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
