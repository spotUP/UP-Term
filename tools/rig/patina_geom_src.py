"""Sources of the host tool tools/rig/patina_geom.c: one list for the gui rig (patina_gui_rig.py) and its host
test (tests/test_patina_geom_term.py), so a new link dependency of the menu code is added in one place."""
GEOM_SRC = ['tools/rig/patina_geom.c', 'patina/menu_geom.c', 'patina/menu_core.c', 'patina/menu_tree.c',
            'patina/menu_icon.c', 'config/keymap.c', 'config/upconf_rows.c', 'render/emoji.c',
            'prefs/settings_def.c', 'prefs/prefs_core.c', 'config/upconf.c', 'handler/kcmode.c', 'handler/slash.c',
            'patina/amconf_keys.c', 'patina/win_core.c']
