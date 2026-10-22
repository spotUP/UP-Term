# Amarchy rig 4 checks, 2026-10-22 (recorded from agent report)

- 8adbb48e (pushed): CxFilter rejects numeric codes in description string -> rows=0. Fix: plain 'rawkey lcommand' filter + SetFilterIX for code/qualifier; cx_ixspec replaces cx_describe. Revert proof test_keymap.c:395; 1051/0. Rig rows=9 after fix.
- Check 1 heap: estimate only. Full dist keymap ~6.2-6.4 KB heap; total ~32.0-32.1 KB vs limit 32768 (600-700 B margin). Second Run refused, no leak.
- Check 5 STATUS/RELOAD PASS (hits=0 rows=8 enabled=1 active=0). Notify auto-reload works. RUN <n> untested.
- amarchy_rig.py 5/7 (r1_commodity, r3_terminal fail). keymap_rig (B5) 1/12. All failures: hotkeys never fire under wasabi key injection (Left Amiga chords). Suspect injection/focus, not Amarchy; unproven.
- Not run: checks 4, 6 (amarchy_menu_rig.py), 7; A6 meta_alt_rig.py, B6, D1. No E1 clash or D1 timing script exists.
- rig.py install does not install Amarchy/amarchy-menu (used ami.py put).
- dist/keymap header does not say user rows need [bind] section.
