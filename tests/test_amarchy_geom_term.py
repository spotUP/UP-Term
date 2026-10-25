#!/usr/bin/env python3
"""test_amarchy_geom_term.py -- the gui rig's expected geometry (tools/rig/amarchy_geom.c) counts only the rows
amarchy-gui shows over Workbench (no tab:/menuid:/slash: rows), as gui.c does when no UP-Term window is under
the menu; --term gives the full menu. Host-only: python3 tests/test_amarchy_geom_term.py"""
import pathlib, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ['tools/rig/amarchy_geom.c', 'amarchy/menu_geom.c', 'amarchy/menu_core.c', 'amarchy/menu_tree.c',
       'amarchy/menu_icon.c', 'config/keymap.c', 'config/upconf_rows.c', 'render/emoji.c']


def levels(exe, *flag):
    out = subprocess.run([exe, *flag, '640', '512', '8', '8', '0', '0'], capture_output=True, text=True, check=True).stdout
    return {l.split()[1]: int(l.split()[6]) for l in out.splitlines() if l.startswith('level ')}


def main():
    exe = str(pathlib.Path(tempfile.mkdtemp(prefix='geomterm')) / 'geom')
    subprocess.run(['cc', '-std=c99', '-I', 'build/amiga', '-o', exe] + SRC, cwd=ROOT, check=True)
    wb, term = levels(exe), levels(exe, '--term')
    ok = wb['root'] == 1 and term['root'] > wb['root']
    print('workbench root rows %d (want 1: Apps; Games comes from the index), with terminal %d: %s' % (wb['root'], term['root'], 'ok' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
