#!/usr/bin/env python3
"""test_menu_on_move.py -- THEME-PREVIEW-PER-KEY: the text menu's theme preview runs on every cursor-move key.

  python3 -I tests/test_menu_on_move.py

amarchy-menu is Amiga-only, so this checks the two links the per-key recolour needs and fails when one is missing:
  1. the libupkgui commit pinned in tools/upkgui.rev exports ui_on_move (the cursor-move hook of ui_run);
  2. amarchy/menu.c installs on_move with ui_on_move on the menu path (next to every ui_on_tick(on_tick)), and on_move
     calls preview_sync.
Exit 1 on a missing link."""
import pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
menu = (ROOT / 'amarchy' / 'menu.c').read_text()

m = re.search(r'static void on_move\([^)]*\)\s*\{(.*?)^\}', menu, re.S | re.M)
if not m or 'preview_sync(0)' not in m.group(1):
    sys.exit('FAIL: menu.c has no on_move that calls preview_sync(0)')
ticks = len(re.findall(r'ui_on_tick\(on_tick, 0\)', menu))
moves = len(re.findall(r'ui_on_move\(on_move, 0\)', menu))
if moves < 2:   # after ui_start, and again after a job view replaced the tick
    sys.exit('FAIL: menu.c sets ui_on_move %d times for %d ui_on_tick(on_tick) sites' % (moves, ticks))

rev = (ROOT / 'tools' / 'upkgui.rev').read_text().strip()
common = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                        capture_output=True, text=True).stdout.strip()
ports = pathlib.Path(common).parent.parent / 'upterm-ports'   # the peer checkout next to the main vtcon checkout
hdr = subprocess.run(['git', '-C', str(ports), 'show', rev + ':pkgs/upkg-ui/src/ui.h'], capture_output=True, text=True)
if hdr.returncode or 'ui_on_move' not in hdr.stdout:
    sys.exit('FAIL: pinned libupkgui %s has no ui_on_move (%s)' % (rev[:8], hdr.stderr.strip()[:80]))
print('OK: menu.c installs ui_on_move; pinned libupkgui %s exports it' % rev[:8])
