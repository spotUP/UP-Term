# Kit names 30: fix (s75), vtcon branch s75-kit-names, commit d78cabde
- Patches: 0015-bebbosshd-sftp.patch, 0022-utf8-vtcon-window.patch (reference in tests/test_dist_bebbossh.py updated).
- Themes (.conf and .yaml): github-light-high, gruvbox-{dark,light}-{hard,soft}, shades-of-purple-dark, zenbones-forest-light. Alias table: pc_theme_alias in prefs/prefs_core.c (prefs_theme_alias), applied in prefs_theme_file (every name lookup: /theme, Amarchy leaf) and prefs_theme_index. dist/install.dos deletes the old files from ENVARC:up-term/themes.
- Packages: up-hexed, up-picom, up-sqlite; dist/package-renames.txt (3 lines) drives Replaces/Conflicts/Provides and record migration (verified _rename_fields output). upterm-ports needed no change.
- Tests: test_dist_name_len, test_theme_aliases (new, wired in Makefile), test_dist_bebbossh, test_dist_packages (count 7->10), test_dist_slim: pass. test_dist_ipk fails identically on main (upterm-ports git recipe has no git_SUMMARY). Host check of the C alias path: old names map correctly.
- Not run: C test_prefs binary (full host build), kit rebuild. Added cases are in tests/test_prefs.c.
