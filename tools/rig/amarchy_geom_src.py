"""Sources of the host tool tools/rig/amarchy_geom.c: one list for the gui rig (amarchy_gui_rig.py) and its host
test (tests/test_amarchy_geom_term.py), so a new link dependency of the menu code is added in one place."""
GEOM_SRC = ['tools/rig/amarchy_geom.c', 'amarchy/menu_geom.c', 'amarchy/menu_core.c', 'amarchy/menu_tree.c',
            'amarchy/menu_icon.c', 'config/keymap.c', 'config/upconf_rows.c', 'render/emoji.c',
            'prefs/settings_def.c', 'prefs/prefs_core.c', 'config/upconf.c', 'handler/kcmode.c', 'handler/slash.c',
            'amarchy/amconf_keys.c', 'amarchy/win_core.c']
