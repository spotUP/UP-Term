#!/usr/bin/env python3
"""mkkit.py -- the shared tail of every archive: the scripts, texts, icons, VERSIONS, modes,
manifest, lha archive and Aminet readme of one package of dist/packages.txt.

  python3 tools/mkkit.py <id>

The Makefile's dist-<id> stages the payload first (build/dist/<archive>/Files/... and
Files/upupdate); this adds everything the archive has in common (kit-split plan D2, K2-7):
  Install, Install.info, Uninstall, Uninstall.info   assembled by tools/mkinstaller.py
  Files/install.dos, README.txt, LICENSES.txt
  Files/VERSIONS            tools/mkversions.sh --package <archive> <version> (packages.txt: consumes)
  Files/UP-Term.version    the base only: "$VER: UP-Term <version> (<date>)", the add-ons' base check
  Files/MANIFEST            tools/mkmanifest.py with the package's parts.txt
  build/<archive>.lha       and build/<archive>.readme (tools/mkreadme.py)
UPTERM_ROOT (environment) is where the other repositories are (default: the parent of vtcon).
"""
import datetime, os, pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import libguard, mkinstaller, pkgs, slim

BUILD = ROOT / 'build'


def templates(pkg_id):
    """(Install, install.dos, Uninstall, README, LICENSES, parts) template paths of the package."""
    if pkg_id == 'base':
        d = ROOT / 'dist'
        return (d / 'Install.installer', d / 'install.dos', d / 'Uninstall', d / 'README.txt',
                d / 'LICENSES.txt', d / 'parts.txt')
    a = ROOT / 'dist/addons' / pkg_id
    return (a / 'Install.installer', a / 'install.dos', ROOT / 'dist/common/uninstall-addon.dos',
            a / 'README.txt', a / 'LICENSES.txt', a / 'parts.txt')


def run(*cmd, **kw):
    subprocess.run([str(c) for c in cmd], check=True, cwd=ROOT, **kw)


def write(path, text):
    pathlib.Path(path).write_bytes(text.encode('latin-1'))


def main(argv):
    if len(argv) != 1:
        sys.exit(__doc__)
    pkg_id = argv[0]
    row = {r['id']: r for r in pkgs.rows()}.get(pkg_id) or sys.exit('mkkit: no package %r' % pkg_id)
    archive, version = row['archive'], row['version']
    kit = BUILD / 'dist' / archive
    # the base's archive is up-term, its product is UP-Term: the Installer title, VERSIONS and $VER say the product
    product = 'UP-Term' if pkg_id == 'base' else archive
    if not (kit / 'Files').is_dir():
        sys.exit('mkkit: %s is not staged (make dist-%s)' % (kit / 'Files', pkg_id))
    ixnet = kit / 'Files/libs/ixnet.library'
    if ixnet.is_file():
        libguard.require_clean_ixnet(ixnet)
    inst, dos, unin, readme, lic, parts = templates(pkg_id)
    write(kit / 'Files/install.dos', mkinstaller.build(dos, pkg_id))
    write(kit / 'Install', mkinstaller.build(inst, pkg_id))
    write(kit / 'Uninstall', mkinstaller.build(unin, pkg_id))
    for src, name in ((readme, 'README.txt'), (lic, 'LICENSES.txt')):
        (kit / name).write_bytes(src.read_bytes())
    run('python3', 'tools/mkicon.py', kit / 'Install.info', '--tool', 'Installer', '--plain',
        '--tooltype', 'APPNAME=' + product, '--tooltype', 'MINUSER=AVERAGE', '--tooltype', 'DEFUSER=AVERAGE')
    run('python3', 'tools/mkicon.py', kit / 'Uninstall.info', '--tool', 'C:IconX', '--plain')
    root = os.environ.get('UPTERM_ROOT') or str(ROOT.parent)
    with open(kit / 'Files/VERSIONS', 'wb') as f:
        run('sh', 'tools/mkversions.sh', '--package', product, version, root, *row['consumes'].split(','), stdout=f)
    if pkg_id == 'base':
        date = datetime.date.today().strftime('%d.%m.%Y')
        write(kit / 'Files/UP-Term.version', '$VER: %s %s (%s)\n' % (product, version, date))
    # every file's mode by one rule (lha carries it to the Amiga as protection bits: a file without
    # u+w cannot be replaced by the next Install or an Update); stops on one left without u+w
    run('python3', 'tools/kitmodes.py', kit)
    # last: every file of the archive, its part and CRC, for the Installer's update
    run('python3', 'tools/mkmanifest.py', kit, parts, kit / 'Files/install.dos', '--package', pkg_id)
    if pkg_id in slim.ids():
        # upkg-only: the staged tree is the .ipk's payload (tools/pkgs.py ipk); no lha, no Aminet readme
        print('%s: staged for the .ipk only' % kit)
        return
    lha = BUILD / (archive + '.lha')
    lha.unlink(missing_ok=True)
    subprocess.run(['lha', '-aq', str(lha), archive], check=True, cwd=BUILD / 'dist')
    run('python3', 'tools/mkreadme.py', pkg_id)
    print('%s: %d bytes' % (lha, lha.stat().st_size))


if __name__ == '__main__':
    main(sys.argv[1:])
