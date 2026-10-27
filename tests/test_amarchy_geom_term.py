#!/usr/bin/env python3
"""test_amarchy_geom_term.py -- the gui rig's expected geometry (tools/rig/amarchy_geom.c) counts only the rows
amarchy-gui shows over Workbench (no tab:/menuid:/slash: rows), as gui.c does when no UP-Term window is under
the menu; --term gives the full menu. Host-only: python3 tests/test_amarchy_geom_term.py"""
import pathlib, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ['tools/rig/amarchy_geom.c', 'amarchy/menu_geom.c', 'amarchy/menu_core.c', 'amarchy/menu_tree.c',
       'amarchy/menu_icon.c', 'config/keymap.c', 'config/upconf_rows.c', 'render/emoji.c',
       'prefs/settings_def.c', 'prefs/prefs_core.c', 'config/upconf.c', 'handler/kcmode.c', 'handler/slash.c', 'amarchy/amconf_keys.c']


def levels(exe, *flag):
    out = subprocess.run([exe, *flag, '640', '512', '8', '8', '0', '0'], capture_output=True, text=True, check=True).stdout
    return {l.split()[1]: int(l.split()[6]) for l in out.splitlines() if l.startswith('level ')}


def build(init):
    exe = str(pathlib.Path(tempfile.mkdtemp(prefix='geomterm')) / 'geom')
    subprocess.run(['cc', '-std=c99', '-I', 'build/amiga', '-ftrivial-auto-var-init=' + init, '-o', exe] + SRC, cwd=ROOT, check=True)
    return exe


def run_all(exe):
    return subprocess.run([exe, '--wb', '--dopus', '640', '256', '8', '8', '22', '44'], capture_output=True, text=True, check=True).stdout


def uninitialised_screen_ok():
    """amarchy_geom must not read an uninitialised mg_screen (s.minimal picked the minimal-menu metrics: window 4 rows short).
    The same source built so that stack garbage is 0xAA..., and built so it is 0, must print the same numbers."""
    a, b = run_all(build('pattern')), run_all(build('zero'))
    ok = a == b
    print('geometry with stack garbage == geometry with zeroed stack: %s' % ('ok' if ok else 'FAIL'))
    return ok


def main():
    exe = str(pathlib.Path(tempfile.mkdtemp(prefix='geomterm')) / 'geom')
    subprocess.run(['cc', '-std=c99', '-I', 'build/amiga', '-o', exe] + SRC, cwd=ROOT, check=True)
    wb, term = levels(exe), levels(exe, '--term')
    ok = wb['root'] == 1 and term['root'] > wb['root']
    print('workbench root rows %d (want 1: Apps; Games comes from the index), with terminal %d: %s' % (wb['root'], term['root'], 'ok' if ok else 'FAIL'))
    ok2 = uninitialised_screen_ok()
    return 0 if ok and ok2 else 1


if __name__ == '__main__':
    sys.exit(main())
