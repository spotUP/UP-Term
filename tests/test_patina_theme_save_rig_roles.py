#!/usr/bin/env python3
"""test_patina_theme_save_rig_roles.py -- the Workbench pen check of patina_theme_save_rig expects the desk_role colours
desk_palette writes (S84), not the raw theme hex. Values measured on rigs: vantablack dim 505050 -> 7a7a7a, fg ffffff -> bebebe;
starship dim 98989d -> 8e8e93.

  python3 -I tests/test_patina_theme_save_rig_roles.py     (builds the host desk_expect with cc; exit 1 on failure)"""
import pathlib, sys, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'patina_window_rig', 'upkg_rig',
             'patina_gui_rig', 'patina_tile_rig', 'patina_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
import patina_theme_save_rig as t

bad = []
v, s = t.theme_roles('vantablack'), t.theme_roles('starship')
if len(v) != 4 or v[1] != '7a7a7a' or v[2] != 'bebebe':
    bad.append('vantablack roles %s' % v)
if len(s) != 4 or s[1] != '8e8e93':
    bad.append('starship roles %s' % s)
for b in bad:
    print('[FAIL] ' + b)
print('[FAIL]' if bad else '[OK] theme_roles match the rig-measured desk_role colours')
sys.exit(1 if bad else 0)
